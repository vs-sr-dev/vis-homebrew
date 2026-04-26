/*
 * WolfVIS A.22 — Pickups (medkit, clip, treasures, 1up).
 *
 * Reciprocal of A.20's damage loop: same-tile proximity grab restores
 * health / ammo or bumps score, with vanilla gates (hp==100 blocks
 * health items, ammo==99 blocks clips). Uses the existing decoration
 * sprite/spawn pipeline (Object slot, painter sort, DrawSpriteWorld)
 * with 8 new sprite slots and a small map plane1 obj-id branch.
 *
 * Mapping (vanilla statinfo[] WL_ACT1.C:22..114, GetBonus WL_AGENT.C:667):
 *   obj 47 -> SPR_STAT_24 chunk 26 (food)      PK_HEALTH_10  (+10 hp)
 *   obj 48 -> SPR_STAT_25 chunk 27 (medkit)    PK_HEALTH_25  (+25 hp)
 *   obj 49 -> SPR_STAT_26 chunk 28 (clip)      PK_AMMO_CLIP  (+8 ammo)
 *   obj 52 -> SPR_STAT_29 chunk 31 (cross)     PK_TREAS_100  (+100 score)
 *   obj 53 -> SPR_STAT_30 chunk 32 (chalice)   PK_TREAS_500  (+500)
 *   obj 54 -> SPR_STAT_31 chunk 33 (bible)     PK_TREAS_1K   (+1000)
 *   obj 55 -> SPR_STAT_32 chunk 34 (crown)     PK_TREAS_5K   (+5000)
 *   obj 56 -> SPR_STAT_33 chunk 35 (1up)       PK_HEALTH_FULL(+99 hp + 25 ammo)
 *
 * Sprite slots 70..77 carry the 8 pickup chunks (sparse, no rotation —
 * decorations have enemy_dir == OBJ_DIR_NONE so painter sort + draw
 * use sprite_idx as-is). Object struct gains a pickup_kind BYTE; on
 * grab, sprite_idx is set to -1 which DrawSpriteWorld already treats
 * as "skip" via its bounds check, and painter sort can short-circuit.
 *
 * Vanilla gates: medkit + food do nothing if hp == 100, clip does
 * nothing if ammo == 99. The pickup stays on the floor for a later
 * grab. Vanilla also ungates the 1up (always grabbed) and treasures
 * (no cap on score). We mirror this exactly.
 *
 * NOT in scope (defer):
 *   - bo_alpo (+4 hp dog food, gated == 100): same as bo_food but
 *     barely useful, not visible on E1L1.
 *   - bo_gibs (+1 hp, gated < 10): emergency heal, not visible on L1.
 *   - bo_machinegun / bo_chaingun: weapon upgrades require A.17 weapon
 *     class state expansion. Defer to a later milestone.
 *   - bo_key1 / bo_key2: door unlock requires the locked-door variant
 *     pattern. L1 has no locked doors so safe to skip.
 *   - bo_spear (Spear of Destiny): SOD-specific, WL1 shareware doesn't
 *     have it.
 *   - GiveExtraMan / treasurecount / EXTRAPOINTS lives bonus: lives
 *     system not modeled in PoC. 1up gives hp + ammo only.
 *
 * --- Original A.20.1 header (kept for traceability) ---
 *
 * WolfVIS A.20.1 — Combat fairness + damage flash polish.
 *
 * Two targeted tweaks on top of A.20 after first-test verdict
 * "structure perfect, feel punitive (CIPA — no sensory feedback)
 * + suspicion of shoot-through-walls". Two diagnoses tested in this
 * iter; wall-shoot deferred pending re-test post-polish.
 *
 * What changes vs A.20:
 *   - ShootPlayer hitchance switches to the vanilla "visible" branch
 *     (160 - dist*16) instead of the "not visible" branch (256 - dist*8).
 *     Reasoning: when LOSCheck passes, the enemy can see the player AND
 *     the player can see the enemy (LOS is symmetric on a tile grid).
 *     That's exactly the vanilla FL_VISABLE case where the player has
 *     a chance to dodge — vanilla halves the hitchance accordingly.
 *     Net at dist=2: 240 (94%) -> 128 (50%). At dist=4: 224 (87%) ->
 *     96 (38%). At dist=6: 208 (81%) -> 64 (25%). Combat becomes
 *     dodgeable instead of point-blank deterministic.
 *   - Damage flash. New g_damage_flash_ticks counter (set to 3 on hit
 *     in DamagePlayer) drives a red border drawn around the viewport
 *     rect after DrawCrosshair. Decremented per InvalidatePlayerView,
 *     painted only while > 1 (so the final tick is a clear frame —
 *     guarantees the red is wiped even if the player is idle when the
 *     count hits 0). WM_TIMER forces moved=TRUE while the counter is
 *     > 0 so the flash + clear frames render even without input.
 *     Border (5 px wide, color 40 = HUD_FG_LOW red) leaves the central
 *     viewport unobstructed — player still sees enemies during the
 *     flash, but knows they took damage at viewport-edge focal vision.
 *
 * Defer: wall-shoot suspicion needs more user data. LOSCheck is tile-
 * grid Bresenham (A.16b memo); sub-tile precision upgrade is a separate
 * iter (A.20.2 if confirmed) since it touches a hot path.
 *
 * --- Original A.20 header (kept for traceability) ---
 *
 * WolfVIS A.20 — Enemy fire-back + player health (gameplay loop closure).
 *
 * Closes the symmetric combat loop: guards now shoot the player back, hit
 * rolls scaled by tile distance, damage chips a 0..100 HP pool tracked in
 * the HUD HEALTH panel that A.21 had pre-baked. On death the player freezes
 * + HEALTH=000 in red; tapping PRIMARY restarts the level (reset hp/ammo/
 * score/kills + respawn at map plane1 marker + reset all enemy state).
 *
 * Mirror of A.18 (player -> enemy hitscan): every piece reuses an existing
 * pattern, no new render or input subsystem. The whole milestone is in
 * AdvanceEnemies (shoot trigger + SHOOT state ticker) + ShootPlayer +
 * DamagePlayer + RedrawHUDHealth + RestartLevel + a sprite-picker branch
 * + a WM_KEYDOWN branch. ~210 LOC delta.
 *
 * Vanilla source mapping (recon at S18 open):
 *   - WL_ACT2.C s_grdshoot1..3   : 3-frame anim @ 20 tics/phase, T_Shoot
 *     fires on phase 1 (s_grdshoot2.action). After phase 2 returns to
 *     s_grdchase1.
 *   - WL_ACT2.C T_Chase shoot trigger (line 3079..3140) : CheckLine LOS
 *     + chance roll. dist == 0 or (dist==1 && distance<0x4000) -> chance
 *     = 300 (always); else chance = (tics<<4)/dist. We translate tics to
 *     elapsed_ms / 14 (vanilla 70-Hz tic rate equivalent).
 *   - WL_ACT2.C T_Shoot (line 3444) : hitchance = 256 - dist*8 (player
 *     not running, not visible). Damage table tiered by dist:
 *       dist < 2 -> US_RndT()>>2 (0..63)
 *       dist < 4 -> US_RndT()>>3 (0..31)
 *       else     -> US_RndT()>>4 (0..15)
 *   - WL_AGENT.C TakeDamage (line 386) : subtract, clamp 0, ex_died on
 *     reaching 0. HealSelf caps at 100.
 *   - WL_AGENT.C DrawHealth : LatchNumber(21,16,3,health). HUD panel
 *     pre-baked at x=193 in A.21, 3 digits, init 100.
 *
 * What changes vs A.21 (single-pass additive edits):
 *   - NUM_SPRITES 67 -> 70. New slots GRD_SHOOT1..3 hardcoded at VSWAP
 *     chunks 96/97/98 (sequential after PAIN/DIE/DEAD at 90..95, vanilla
 *     WL_DEF.H:208).
 *   - New OBJ state OBJ_ST_SHOOT = 4. Phase 0..2 at GUARD_SHOOT_PHASE_MS
 *     (286 ms = 20 tics @ 70Hz). On phase 1 -> ShootPlayer(idx). After
 *     phase 2 -> revert to OBJ_ST_WALK.
 *   - AdvanceEnemies: live STAND/WALK guards roll a shoot chance every
 *     tick using the vanilla T_Chase formula. SHOOT state has its own
 *     phase ticker. Movement is suppressed during SHOOT.
 *   - ShootPlayer(idx, hWnd): mirror of FireHitscan but reverse direction.
 *     Tile-grid distance = max(|dx|,|dy|), hitchance = 256-dist*8 (PoC
 *     skips visibility distinction since we don't track FL_VISABLE).
 *     Damage roll matches vanilla tiered table.
 *   - DamagePlayer(dmg, hWnd): subtract from g_player_hp, clamp 0, set
 *     g_player_dead, RedrawHUDHealth.
 *   - RedrawHUDHealth(hWnd): clone of RedrawHUDAmmo at HUD_HEALTH_X=193.
 *     HUD_FG_LOW (red) when hp < 25.
 *   - DrawAllSprites SHOOT branch: slot = GRD_SHOOT1_SLOT + state_phase,
 *     non-rotating (vanilla statetype.rotate=false on s_grdshoot*).
 *   - WM_KEYDOWN PRIMARY: when g_player_dead -> RestartLevel(hWnd) which
 *     resets all gameplay state + invalidates the viewport. Else fire as
 *     before.
 *   - WinMain class name "WolfVISa22" + window title to match.
 *
 * Foundation invariants (do NOT regress from A.21):
 *   - DispDib BEGIN/END video-mode framing + direct A000 blit unchanged.
 *   - All A.18..A.21 weapon/AI/sprite/door/HUD subsystems untouched
 *     except the AdvanceEnemies extension and the small DrawAllSprites
 *     branch addition.
 *   - Recon-first 13-for-13 streak — A.20 is single-iter target.
 *
 * --- Original A.21 header (kept for traceability) ---
 *
 * WolfVIS A.21 — direct A000 framebuffer + DispDib BEGIN/END (PF finale).
 *
 * Replaces the per-frame StretchDIBits + Select/RealizePalette path with a
 * one-time DispDib BEGIN at startup, direct _fmemcpy to the A000:0000
 * selector each frame, and DispDib END at WM_QUIT. Per-frame GDI cost is
 * gone. Preserves bottom-up DIB convention by walking src bottom-up while
 * dest goes top-down inside FlushFramebufToA000. The selector idiom
 * (low WORD of &_A000H far ptr) is the offset-of-symbol patched by the
 * Win16 loader at boot — see reference_a000h_idiom.md for why.
 *
 * --- Original A.19.2 header (kept for traceability) ---
 *
 * WolfVIS A.19.2 — micro-perf bundle (PF finale step 3).
 *
 * Two micro-wins on top of A.19.1, both targeting the inner hot paths
 * where A.19/A.19.1 left residual cycles. Single-iter zero-fix track
 * record continues; both patterns are mechanical refactors of code
 * that already runs correctly.
 *
 * What changes vs A.19.1 (additive, two functions touched):
 *   - DrawWallStripCol ceiling/wall/floor fills do WORD pair-writes.
 *     Replaces `*fb1 = c; *fb2 = c;` (two byte stores, ~6 cyc/pair on
 *     286) with `*((WORD __far *)fb1) = pair;` (single aligned word
 *     store, ~3 cyc/pair). VIEW_X0=96 + half-col cast step=2 keep sx
 *     even, so all paired writes are word-aligned. CEIL_PAIR /
 *     FLOOR_PAIR / wall pair_w precomputed once per fill region.
 *     ~10 ms/frame across all visible columns at MAME-VIS rate.
 *   - DrawSpriteWorld outer column loop pre-clips dest_col bounds
 *     ONCE per sprite. Prior code iterated dest_col = dest_left ..
 *     dest_right (could be -200..312 for close sprites = 512 iter)
 *     and skipped off-viewport cols via per-iter `continue` after
 *     advancing srcx_acc. Now col_iter_start = max(dest_left, 0) and
 *     col_iter_end = min(dest_right, VIEW_W) are computed once;
 *     srcx_acc is seeded to (col_iter_start - dest_left) * step_q16
 *     so the first iter samples the same source column as the
 *     pre-clip code did. Inner loop loses the bound check entirely.
 *     ~3-4 ms/frame per close sprite.
 *
 * Foundation invariants (do NOT regress):
 *   - All A.19 / A.19.1 invariants (centered viewport, minimap toggle,
 *     Q.16 sprite scaler).
 *   - DrawWallStripCol Q.16 wall sampling (A.13.1) unchanged — the
 *     pair-write change is purely on the store side.
 *   - Z-buffer occlusion test unchanged.
 *   - Sprite post format unchanged.
 *
 * Defer to A.21+:
 *   - Top-down DIB orientation flip (recon: ~60 cyc/frame in setup
 *     alone, hot path uses fb +=/-= SCR_W either way — too invasive
 *     for marginal real perf, defer indefinitely).
 *   - Split telemetry (5 PIT-direct sub-counters for cast / wall /
 *     sprite / overlay / paint). Now meaningful post-A.19/.1/.2.
 *   - EnterDVA direct A000:0000 framebuffer (eliminates StretchDIBits
 *     entirely — programmers_ref.txt API recon already done in S15).
 *
 * --- Original A.19.1 header (kept for traceability) ---
 *
 * WolfVIS A.19.1 — sprite scaler accumulator (PF finale step 2).
 *
 * Closes the close-enemy freeze (H1) reported in A.19. DrawSpriteWorld
 * inner loop replaced its per-pixel long division with the Q.16 step
 * accumulator pattern already canonical in DrawWallStripCol — same
 * primitive, mirrored shape. When a guard is at near-clip distance
 * the projected sprite_h saturates at 4*VIEW_H = 512 px; the original
 * inner loop did `sy_src = (long)(dy - dy_top) * 64L / sprite_h` for
 * every pixel = ~80 visible cols * 512 px * ~250 cyc per pixel = ~10M
 * cyc per sprite per frame, perceived as a hard freeze (>1 s). The
 * accumulator pattern brings the per-pixel cost down to ~5-10 cyc
 * (one add + one shift + one bounded byte write).
 *
 * What changes vs A.19 (additive, single function rewritten):
 *   - DrawSpriteWorld inner loops use Q.16 fixed-point step:
 *       step_q16 = (64L << 16) / sprite_h    [once per sprite]
 *       srcx_acc = 0; advances by step_q16 per dest_col
 *       sy_acc   = step_q16 * (dy_iter_start - dy_top); per pixel +=
 *     Per-pixel cost: 1 shift to extract sy_src, 1 add for next sy_acc,
 *     1 byte store via decrementing far pointer (no per-pixel mul).
 *   - dy bounds clipped ONCE per post (dy_iter_start = max(dy_start,
 *     VIEW_Y0); dy_iter_end = min(dy_end, VIEW_Y0 + VIEW_H)) so the
 *     inner loop is bound-check-free.
 *   - framebuf access via decrementing __far pointer (matches the
 *     wallstrip pattern in DrawWallStripCol; bottom-up DIB so the
 *     pointer DECREMENTS by SCR_W per dy++).
 *   - sy_src clamping kept defensively: `if (sy >= endy_src) sy =
 *     endy_src - 1;` guards Q.16 rounding from sampling past the post
 *     end. Cost: 1 cmov-equivalent per pixel (~1-2 cyc on 286).
 *
 * Foundation invariants (do NOT regress):
 *   - All A.19 invariants (centered viewport, minimap toggle on F1).
 *   - DrawWallStripCol unchanged — it already uses the accumulator
 *     pattern. The cost asymmetry (walls fast, sprites slow) is the
 *     reason A.18+ guards "froze" instead of slowing — fixed here.
 *   - Z-buffer occlusion test unchanged (`g_zbuffer[dest_col] <=
 *     cam_y`).
 *   - Sprite post format unchanged: `t_compshape` triples (endy*2,
 *     corr_top, starty*2) terminated by endy*2 == 0.
 *   - HUD bake (A.15) untouched.
 *
 * Defer to A.19.2 / A.20+:
 *   - Top-down DIB orientation flip (eliminates fb_y inversion in
 *     remaining FB_Put callers — minimap mode + crosshair + HUD).
 *   - WORD ceiling/floor fills in DrawWallStripCol (paired byte
 *     writes -> 1 word write).
 *   - Split telemetry (5 PIT-direct sub-counters for cast / wall /
 *     sprite / overlay / paint).
 *   - EnterDVA direct A000:0000 framebuffer (eliminates StretchDIBits
 *     entirely — programmers_ref.txt API recon already done).
 *   - Enemy firing back + player health (was original A.19 scope).
 *
 * --- Original A.19 header (kept for traceability) ---
 *
 * WolfVIS A.19 — centered viewport + minimap toggle (PF finale step 1).
 *
 * Layout rework + first PF win. Removes the per-frame minimap cost
 * (DrawMinimapWithPlayer was 4096 FB_Put calls per frame, each with
 * 4 bound checks + a long multiplication for framebuf addressing —
 * estimated ~25-30 ms/frame on MAME-VIS). Centers the 128x128
 * raycaster viewport horizontally on the 320-wide screen so the
 * left/right margins are symmetric instead of viewport-on-the-left
 * + minimap-on-the-right. Adds a minimap toggle bound to VK_HC1_F1
 * (Xbox X / hand controller "third" button) so the minimap remains
 * available on demand without paying the per-frame cost.
 *
 * What changes vs A.15.1 (additive):
 *   - VIEW_X0 0 -> 96. Viewport now occupies x=96..223 (128 px) of
 *     SCR_W=320. Margins x=0..95 and x=224..319 in the viewport
 *     row band (y=35..162) are left at the ClearFrame boot color
 *     (black) — never overwritten per frame, zero cost. Crosshair
 *     and weapon overlay positions track VIEW_X0 automatically (both
 *     use VIEW_X0 + VIEW_W/2 etc., constants propagate).
 *   - g_show_minimap BOOL toggle, default FALSE. When TRUE, the
 *     viewport rect is repainted with a centered 1:1 64x64 minimap
 *     (same content as DrawMinimapWithPlayer in A.15.1: tiles +
 *     objects + door state + player dot + heading) instead of the
 *     raycaster pass.
 *   - Input rebind. VK_HC1_F1 (0x71, Xbox X button on MAME) toggles
 *     g_show_minimap. The A.10..A.15.1 music F1=Start/F3=Stop debug
 *     bindings are removed; the OPL/IMF subsystem itself stays in
 *     the binary (sqActive=FALSE means zero per-frame cost) for
 *     future re-enable. F3 is reserved for a strafe toggle in a
 *     follow-up milestone.
 *   - InvalidatePlayerView dispatch. Branches on g_show_minimap:
 *     minimap path -> DrawMinimapView(); raycaster path ->
 *     DrawViewport() + DrawCrosshair(). Crosshair is suppressed in
 *     minimap mode (would overlap the map and distract).
 *   - DrawMinimapView(): fills the 128x128 viewport rect with HUD_BG
 *     (dark grey), then draws the 64x64 minimap centered (origin at
 *     x=VIEW_X0+32, y=VIEW_Y0+32) using the same tile/object/door
 *     palette as DrawMinimapWithPlayer plus player dot + heading
 *     line. Sprite-style cell at 1:1 = readable on 320-wide TV.
 *   - Dirty rect in InvalidatePlayerView reduced to viewport-only
 *     (was VIEW_X0..MINIMAP_X0+MAP_W to cover the right-side mini-
 *     map; now MINIMAP_X0/MAP_W are unused in the per-frame path).
 *
 * Foundation invariants (do NOT regress):
 *   - All A.15.1 / A.18 / A.16b / A.13.1 invariants (renderer + AI +
 *     HUD + raycaster).
 *   - HUD bake (A.15) untouched — the y=163..199 row band is below
 *     the viewport rect and never overwritten per frame.
 *   - DDA cardinal-angle nudge (A.15.1 fix) preserved.
 *   - gamepal.h LOOKUP MANDATORY for any new color choice.
 *
 * Defer to A.19.x / A.20+:
 *   - Strafe toggle on F3 (movement quality polish).
 *   - DispDib / EnterDVA bypass of GDI StretchDIBits — programmer's
 *     ref doc has the API; future milestone for direct A000:0000
 *     framebuffer access (potential 30-50% speedup).
 *   - Split telemetry (per-subsystem ms PIT counter).
 *   - Sprite scaler accumulator (per-pixel division -> step add).
 *   - Top-down DIB orientation flip (eliminates fb_y inversion).
 *   - Enemy firing back + player health (was original A.19 scope,
 *     now slated for A.20).
 *
 * --- Original A.15.1 header (kept for traceability) ---
 *
 * WolfVIS A.15.1 — real BJ Blazkowicz face on HUD via VGAGRAPH
 * chunked Huffman. Replaces the A.15 primitive helmet placeholder
 * with the canonical SPR_GRD_FACE1APIC (full-health, looking
 * straight) loaded from VGAGRAPH.WL1. FACE1APIC = chunk 121 on the
 * shipped WL1 pictable (NOT 113 as GFXV_WL1.H enum says — the
 * shipped asset was rebuilt with extra interstitial pics).
 * Bundled cardinal-angle DDA nudge fix in DrawViewport cast loop
 * (centers ra by 1 unit when it would land exactly on a cardinal
 * axis — eliminates the spurious 1-px wall column at viewport
 * center on E/N/W/S exact heading).
 *
 * --- Original A.18 header (kept for traceability) ---
 *
 * WolfVIS A.18 — firing + hitscan + damage (gameplay loop closure).
 *
 * Closes the "horror stare" gap from A.16b: PRIMARY now fires the
 * pistol, hitscan ray finds the closest visible enemy in front of
 * the crosshair, damage drops hp, dying guards play the 3-frame DIE
 * animation and freeze on DEAD. Score and ammo on the HUD update on
 * fire and on kill via partial-rect re-blit (the static-bg HUD bake
 * from A.15 is not invalidated; only the digit boxes are repainted
 * in-place).
 *
 * What changes vs A.17 (additive):
 *   - Sprite slot extension. NUM_SPRITES 59 -> 67. New runtime-patched
 *     slots 59..62 = SPR_PISTOLATK1..4 (chunks total - 14..-11, same
 *     end-of-VSWAP discovery pattern as A.17). New compile-time
 *     entries 63..65 = SPR_GRD_DIE_1..DIE_3 (absolute enum 91..93)
 *     and 66 = SPR_GRD_DEAD (enum 95). Pain frames skipped per PoC
 *     scope (vanilla pain is a one-frame flash that flips back to
 *     chase; not load-bearing for the gameplay loop).
 *   - Input rebind. PRIMARY (VK_HC1_PRIMARY) now triggers FireWeapon();
 *     SECONDARY (VK_HC1_SECONDARY) takes over door-toggle duty (the
 *     A.14.1 OPL sanity click on SECONDARY is dropped). Movement keys
 *     and music keys unchanged.
 *   - Weapon FSM. Three states: READY (idle, draws SPR_PISTOLREADY),
 *     FIRING (cycles ATK1..ATK4 every WEAPON_PHASE_MS=100 ms then
 *     auto-returns to READY). FireWeapon() gates on weapon_state ==
 *     READY && g_ammo > 0; on accept it decrements ammo, advances to
 *     FIRING phase 0, and triggers FireHitscan(). AdvanceWeapon() in
 *     WM_TIMER does the time-scaled phase progression.
 *   - Hitscan. FireHitscan() walks g_objects[], projects each living
 *     enemy into camera space (same math as DrawSpriteWorld:
 *     cam_y = rx*cos+ry*sin, cam_x = -rx*sin+ry*cos, both Q8.8) and
 *     tests whether VIEW_W/2 (the crosshair column) falls inside the
 *     sprite's projected screen span [screen_x - sprite_h/2, +/2].
 *     Walls block bullets via the existing g_zbuffer[VIEW_W/2] which
 *     CastRay populates each frame; no separate ray cast needed for
 *     hitscan. Closest passing enemy is the hit; ties broken by
 *     smallest cam_y_q88.
 *   - Damage + state transitions. Object grows BYTE hp (init 25 for
 *     guards = vanilla GUARD_HP). Damage roll = 5 + (g_step_q88 PRNG
 *     >> 13) & 7 = 5..12, close to vanilla US_RndT()&7 + BASEDAMAGE.
 *     hp <= 0 -> state OBJ_ST_DIE, phase 0, tick_last = now. The DIE
 *     state plays 3 frames (DIE_1, DIE_2, DIE_3) at ENEMY_PHASE_MS
 *     each, then transitions to OBJ_ST_DEAD frozen on SPR_GRD_DEAD.
 *     PAIN flash skipped per PoC. AdvanceEnemies extended to drive
 *     the DIE phase timer; LOSCheck and chase logic skip dead/dying
 *     enemies (no LOS contribution, no movement). Painter sort still
 *     orders cadavers (corpses must z-sort with live sprites).
 *   - DrawAllSprites sprite picker. STAND/WALK paths unchanged;
 *     adds DIE -> slot 63+state_phase, DEAD -> slot 66. DIE/DEAD
 *     frames are non-rotating (vanilla statetype.rotate=false), so
 *     no atan2/dirangle math applies.
 *   - HUD partial re-blit. RedrawHUDAmmo()/RedrawHUDScore() clear the
 *     2-digit (ammo) / 6-digit (score) box with HUD_BG fill, draw the
 *     new value with DrawNumber, and InvalidateRect the small region.
 *     framebuf is patched in-place; static_bg is left alone (the bake
 *     was only used for first-paint; framebuf's HUD region is not
 *     overwritten by DrawViewport so values persist between fires).
 *   - Game state vars. g_weapon_state, g_weapon_phase, g_weapon_tick,
 *     g_ammo (init 8), g_score (init 0), g_kills (telemetry).
 *
 * Foundation invariants (do NOT regress):
 *   - All A.13.1: grid-line DDA, side-aware TileToWallTex, paired-col
 *     DrawWallStripCol, time-scaled AdvanceDoors, partial-src
 *     StretchDIBits, -ox -s build flags.
 *   - All A.16a: sparse sprite_chunk_offs, dirangle_q10, atan2_q10,
 *     ord_to_dirtype, painter sort over depth_q88.
 *   - All A.16b: x_q88/y_q88 as authoritative position, time-scaled
 *     AdvanceEnemies, per-axis collision, state machine in Object.
 *   - All A.17: mutable sprite_chunk_offs, runtime patch in LoadVSwap,
 *     DrawSpriteFixed for HUD/overlay 1:1 blits.
 *
 * Defer to A.19+:
 *   - Vanilla pain flash (one-frame interrupt of chase on damage).
 *   - Enemy firing back (SPR_GRD_SHOOT1..3 chunks 96..98 + RNG-gated
 *     T_Chase shoot branch + player damage).
 *   - SFX on fire (OPL3 chunks in VSWAP post sprite range).
 *   - Pickups (ammo/score/health drops on guard death).
 *   - Weapon switch (knife/pistol/MG/chaingun frame sets).
 *   - Real BJ face from VGAGRAPH (A.15.1 from S13 reminder).
 *
 * --- Original A.17 header (kept for traceability) ---
 *
 * Visual milestone: real Wolf3D pistol sprite painted over the
 * bottom-center of the viewport every frame, foreground-on-top of
 * the raycaster scene + in-world sprites. Static (no firing animation,
 * no input rebind — those are A.18 scope).
 *
 * Two-iter session: iter-1 shipped a primitive FillRect silhouette
 * (~14 rects, fan-art tier) and proved the overlay layout; iter-2
 * (this) replaces it with the canonical SPR_PISTOLREADY chunk loaded
 * from VSWAP at boot.
 *
 * --- Original A.16b header (kept for traceability) ---
 *
 * First milestone where guards stop being billboards and start being
 *
 * First milestone where guards stop being billboards and start being
 * actors: phase-cycled walking sprites, LOS-aware chase, sub-tile
 * movement, collision against walls + doors. No firing yet (PRIMARY still
 * toggles doors per A.14.1; firing arrives in A.18 with hitscan/damage).
 *
 * What changes vs A.13.1 / A.16a (additive):
 *   - VSWAP loader extended to also load 32 walking-frame chunks at
 *     sprite_start_idx + 58..89. These are SPR_GRD_W1_1..SPR_GRD_W4_8
 *     in WOLFSRC/WL_DEF.H (4 phases x 8 directions). They live at slots
 *     26..57 in our sprites[] array, packed phase-major:
 *       slot 26..33 = W1 phase, 8 directions
 *       slot 34..41 = W2 phase, 8 directions
 *       slot 42..49 = W3 phase, 8 directions
 *       slot 50..57 = W4 phase, 8 directions
 *   - NUM_SPRITES bumped 26 -> 58. Memory: 58 * 4096 = 237 KB on __huge.
 *   - Object struct extended:
 *       BYTE  state            : 0=stand, 1=walk (room reserved 2..4 for
 *                                 future shoot/pain/die/dead in A.18+)
 *       BYTE  state_phase      : 0..3, walking phase index
 *       DWORD state_tick_last  : GetTickCount of last phase advance
 *       fixed x_q88, y_q88     : sub-tile position in 1/256 tile units
 *                                 (replaces tile_x/tile_y for moving
 *                                 enemies; tile_x/tile_y stays as the
 *                                 spawn anchor for the painter sort fast
 *                                 path).
 *   - AdvanceEnemies(): time-scaled state ticker. Mirrors AdvanceDoors
 *     pattern from A.13.1 — uses GetTickCount delta so behavior is
 *     independent of frame rate.
 *       phase advance: 250 ms per phase -> ~1 s per W1->W4 cycle (close
 *         to vanilla path tic durations 20+5+15+20+5+15 / 70 Hz).
 *       movement velocity: ~0.5 tile/sec (close to Wolf3D guard
 *         speed=512/65536 tile/tic * 70 Hz).
 *   - LOSCheck(ex, ey, px, py): tilemap DDA from enemy to player, returns
 *     TRUE if no blocking wall hit. Mirrors WL_STATE.C:1037 CheckLine but
 *     simpler: walls only, doors treated as blocking for PoC (in vanilla
 *     they're transparent if open enough; we defer that to A.18 alongside
 *     hitscan, where the door-aperture math becomes load-bearing).
 *   - DrawAllSprites picks sprite by state:
 *       STAND: sprite_idx = 18 + sector              (existing A.16a path)
 *       WALK:  sprite_idx = 26 + state_phase*8 + sector
 *
 * AI behavior PoC (intentionally minimal):
 *   - All enemies start in STATE_STAND.
 *   - On each tick: if LOSCheck to player passes AND distance to player
 *     > 1.5 tile, switch to STATE_WALK and snap enemy_dir to the 8-way
 *     direction toward the player; advance position by velocity * dt
 *     along that direction. If movement collides with wall/door, abort
 *     and revert to STATE_STAND for this tick.
 *   - If LOS lost, revert to STATE_STAND (position frozen, phase reset).
 *   - No path/patrol behavior (vanilla T_Path is deferred — PoC has only
 *     T_Stand + T_Chase fused into one loop).
 *
 * Defer to A.18 / later:
 *   - Firing animation (SPR_GRD_SHOOT1..3 chunks 96..98), hitscan, damage.
 *   - Pain/Die/Dead frames (chunks 90..95).
 *   - Officer / SS / dog enemy classes.
 *   - Door-aware LOS (transparent when open).
 *   - Vanilla path/patrol behavior.
 *
 * Vanilla source recon backing the design:
 *   - WL_DEF.H:159-208     : sprite enum, GRD_S=50..57, GRD_W1..W4=58..89.
 *   - WL_DEF.H:602-609     : statetype struct (rotate, shapenum, tictime,
 *                            think, action, next).
 *   - WL_ACT2.C:418-444    : guard state defs s_grdstand / s_grdpath1..4 /
 *                            s_grdchase1..4 / s_grdshoot1..3.
 *   - WL_ACT2.C:3047       : T_Stand = SightPlayer only.
 *   - WL_ACT2.C:3069-3367  : T_Chase / T_Path movement + dir selection.
 *   - WL_STATE.C:1037-1185 : CheckLine LOS pattern (door-aware).
 *   - WL_STATE.C:1404-1478 : SightPlayer reaction-delay model.
 *
 * Inherited from A.16a unchanged: 8-direction rotation (atan2_q10 +
 * dirangle_q10), sparse VSWAP table extension pattern, painter's sort
 * over depth_q88, minimap red dot for guards.
 *
 * --- Original A.16a header (kept for traceability) ---
 *
 * First milestone where the world contains living-targets-in-waiting:
 * Wolf3D guards rendered as static billboards at their map_plane1 tile
 * positions in E1L1. Adds 8-direction rotation in iter 2 via atan2 LUT.
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
#include <string.h>   /* _fmemcpy for FlushFramebufToA000 (A.21) */
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

/* Scene layout (A.19: viewport centered on 320 wide -> x=96..223) */
#define DEBUG_BAR_H      30
#define VIEW_X0          96
#define VIEW_Y0          35
#define VIEW_W           128
#define VIEW_H           128
#define VIEW_CY          (VIEW_Y0 + VIEW_H/2)
/* Minimap-mode origin: 64x64 map centered inside the 128x128 viewport
 * rect when g_show_minimap is TRUE (A.19 toggle). Old MINIMAP_X0/Y0
 * (140,35) is no longer referenced — the minimap is no longer painted
 * in a corner per-frame, only fullscreen on F1 toggle. Kept the old
 * defines as harmless compile-time dead code. */
#define MINIMAP_X0       140
#define MINIMAP_Y0       35
#define MINIMAP_TILE_PX  1
#define MINIMAP_VIEW_X0  (VIEW_X0 + (VIEW_W - MAP_W) / 2)   /* 96 + 32 = 128 */
#define MINIMAP_VIEW_Y0  (VIEW_Y0 + (VIEW_H - MAP_H) / 2)   /* 35 + 32 = 67  */
#define MINIMAP_BG       0      /* black fill behind the map */
#define CEIL_COLOR       29     /* mid grey */
#define FLOOR_COLOR      24     /* darker grey */
/* A.19.2 paired-byte constants for WORD pair-writes in DrawWallStripCol.
 * Both bytes of the WORD hold the same color so an aligned word store
 * paints two horizontally-adjacent pixels in a single cycle. */
#define CEIL_PAIR        ((WORD)((CEIL_COLOR  << 8) | CEIL_COLOR))
#define FLOOR_PAIR       ((WORD)((FLOOR_COLOR << 8) | FLOOR_COLOR))

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
/* 59 sprite chunks total in our sprites[] array:
 *   slot 0      = SPR_DEMO        (VSWAP chunk sprite_start_idx + 0)
 *   slot 1      = SPR_DEATHCAM    (VSWAP chunk sprite_start_idx + 1)
 *   slot 2..17  = SPR_STAT_0..15  (VSWAP chunk sprite_start_idx + 2..17)
 *   slot 18..25 = SPR_GRD_S_1..8  (VSWAP chunk sprite_start_idx + 50..57)
 *   slot 26..33 = SPR_GRD_W1_1..8 (VSWAP chunk sprite_start_idx + 58..65)
 *   slot 34..41 = SPR_GRD_W2_1..8 (VSWAP chunk sprite_start_idx + 66..73)
 *   slot 42..49 = SPR_GRD_W3_1..8 (VSWAP chunk sprite_start_idx + 74..81)
 *   slot 50..57 = SPR_GRD_W4_1..8 (VSWAP chunk sprite_start_idx + 82..89)
 *   slot 58     = SPR_PISTOLREADY (VSWAP chunk discovered at runtime as
 *                 (sound_start_idx - sprite_start_idx) - 15; the last
 *                 20 sprite chunks of WL1 shareware are the player
 *                 attack frames in WL_DEF.H:457-468 order: KNIFE x5 +
 *                 PISTOL x5 + MGUN x5 + CHAIN x5. PISTOLREADY is at
 *                 offset +5 inside that range = -15 from end.)
 * Phase-major packing for guards: slot = GUARD_W_FIRST_SLOT + phase*8 + sector.
 * We skip chunks 18..49 (SPR_STAT_16..47 unused on E1L1); the sparse
 * table sprite_chunk_offs[] maps slot -> VSWAP relative chunk index. */
#define NUM_SPRITES           78
#define STAT_SPRITE_COUNT     18    /* slots 0..17 = legacy A.14 statics */
#define GUARD_S_FIRST_SLOT    18    /* slot 18 = SPR_GRD_S_1 (stand, 8 dir) */
#define GUARD_S_FRAME_COUNT   8     /* SPR_GRD_S_1..S_8 */
#define GUARD_S_VSWAP_OFFSET  50    /* SPR_GRD_S_1 = sprite enum value 50 */
#define GUARD_W_FIRST_SLOT    26    /* slot 26 = SPR_GRD_W1_1 (walking) */
#define GUARD_W_PHASE_COUNT   4     /* W1..W4 phases */
#define GUARD_W_VSWAP_OFFSET  58    /* SPR_GRD_W1_1 = sprite enum value 58 */
#define PISTOL_READY_SLOT     58    /* slot 58 = SPR_PISTOLREADY (runtime offset) */
#define PISTOL_READY_OFFSET   15    /* PISTOLREADY = total_sprites - 15 in WL1 */
/* A.18: PISTOLATK1..4 sit immediately after PISTOLREADY in the trailing
 * weapon-arsenal range (KNIFE x5 + PISTOL x5 + MGUN x5 + CHAIN x5).
 * Same runtime-patch pattern as PISTOL_READY_SLOT. */
#define PISTOL_ATK1_SLOT      59
#define PISTOL_ATK2_SLOT      60
#define PISTOL_ATK3_SLOT      61
#define PISTOL_ATK4_SLOT      62
#define PISTOL_ATK1_OFFSET    14    /* PISTOLATK1 = total_sprites - 14 */
/* A.18: SPR_GRD_DIE_1..DIE_3 + SPR_GRD_DEAD live at fixed sprite-enum
 * indices (WL_DEF.H:205-206), NOT in the trailing weapon range. The
 * sparse table sprite_chunk_offs[] carries them as compile-time
 * absolute offsets (no runtime patch). PAIN_1 (90) and PAIN_2 (94)
 * are skipped per PoC scope (see header note). */
#define GRD_DIE1_SLOT         63
#define GRD_DIE2_SLOT         64
#define GRD_DIE3_SLOT         65
#define GRD_DEAD_SLOT         66
#define GRD_DIE1_VSWAP_OFFSET 91    /* SPR_GRD_DIE_1 = sprite enum value 91 */
#define GRD_DEAD_VSWAP_OFFSET 95    /* SPR_GRD_DEAD  = sprite enum value 95 */
/* A.20: SPR_GRD_SHOOT1..3 sit at vanilla enum 96..98 (immediately after
 * PAIN/DIE/DEAD at 90..95, see WL_DEF.H:208). Hardcoded absolute, no
 * runtime patch — same pattern as DIE/DEAD. */
#define GRD_SHOOT1_SLOT       67
#define GRD_SHOOT2_SLOT       68
#define GRD_SHOOT3_SLOT       69
#define GRD_SHOOT1_VSWAP_OFFSET 96  /* SPR_GRD_SHOOT1 = sprite enum value 96 */
#define GUARD_SHOOT_FRAMES    3     /* SHOOT1, SHOOT2, SHOOT3 */

/* A.22: pickup sprite slots. Sparse — only the 8 pickup chunks we
 * actually grant, indexed by absolute sprite enum value (chunk number)
 * matching SPR_STAT_24..33 (vanilla WL_DEF.H:174..175). */
#define PK_FOOD_SLOT      70    /* SPR_STAT_24 chunk 26: food (bo_food)        +10 hp */
#define PK_MEDKIT_SLOT    71    /* SPR_STAT_25 chunk 27: medkit (bo_firstaid)  +25 hp */
#define PK_CLIP_SLOT      72    /* SPR_STAT_26 chunk 28: clip (bo_clip)        +8 ammo */
#define PK_CROSS_SLOT     73    /* SPR_STAT_29 chunk 31: cross (bo_cross)      +100   */
#define PK_CHALICE_SLOT   74    /* SPR_STAT_30 chunk 32: chalice (bo_chalice)  +500   */
#define PK_BIBLE_SLOT     75    /* SPR_STAT_31 chunk 33: bible (bo_bible)      +1000  */
#define PK_CROWN_SLOT     76    /* SPR_STAT_32 chunk 34: crown (bo_crown)      +5000  */
#define PK_FULLHEAL_SLOT  77    /* SPR_STAT_33 chunk 35: 1up (bo_fullheal)     +99hp +25 ammo */

/* PK_* enum values for Object.pickup_kind. PK_NONE for non-pickup
 * objects (decorations, enemies). Branched in TryGiveBonus. */
#define PK_NONE         0
#define PK_AMMO_CLIP    1
#define PK_HEALTH_10    2
#define PK_HEALTH_25    3
#define PK_HEALTH_FULL  4
#define PK_TREAS_100    5
#define PK_TREAS_500    6
#define PK_TREAS_1K     7
#define PK_TREAS_5K     8

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
/* A.22: bumped 128 -> 256 because the pickup branch can push static
 * count past the 128 cap on Wolf3D L1 (decorations + ~20 pickups +
 * 8-15 guards), causing guards at the tail of the row-by-row scan to
 * be silently dropped. Each Object is ~24 B; 256 * 24 = 6 KB __far. */
#define MAX_OBJECTS      256
#define FOCAL_PIXELS     96L    /* (VIEW_W/2) / tan(FOV_ANGLES/2 in rad) */

/* A.15.1 VGAGRAPH chunked Huffman pic loader. WL1 shareware:
 *   VGADICT.WL1   = 1024 B = 256 huffnodes x 4 B (bit0 WORD + bit1 WORD).
 *   VGAHEAD.WL1   =  471 B = 157 entries x 24-bit LE chunk offsets into
 *                   VGAGRAPH.WL1 (last entry = file size sentinel).
 *   VGAGRAPH.WL1  = 326 KB = NUMCHUNKS-1 chunks of (4 B expanded len LE
 *                   followed by Huffman-compressed payload).
 *   chunk 0       = pictable: 144 pictabletype entries (4 B each = w, h
 *                   as int LE). EMPIRICAL count, not the GFXV_WL1.H
 *                   NUMPICS=136 — see Trap S15.1.
 *   FACE1APIC     = chunk 121 EMPIRICAL (NOT 113 as the linker enum
 *                   says). 24x32 pixels, 768 B expanded, ~787 B
 *                   compressed. Stored 4-plane: bytes 0..191 = plane 0
 *                   (cols 0,4,8,12,16,20), 192..383 = plane 1, etc. */
#define VGA_DICT_BYTES        1024
#define VGA_HUFF_NODES        256
#define VGA_HUFF_HEAD         254     /* root node always 254 in EGADICT */
#define VGA_HEAD_BYTES        471     /* WL1 shareware actual file size */
#define VGA_HEAD_ENTRIES      157     /* VGA_HEAD_BYTES / 3 (24-bit) */
#define VGA_FACE1A_CHUNK      121     /* EMPIRICAL — NOT 113 from enum */
#define FACE_W                24
#define FACE_H                32
#define FACE_BYTES            768     /* FACE_W * FACE_H */
#define FACE_PLANE_BYTES      192     /* FACE_BYTES / 4 */
#define FACE_ROW_BYTES_PLANE  6       /* FACE_W / 4 */
#define FACE_HUD_X            148     /* mirror A.15 placeholder x */
#define FACE_HUD_Y            166     /* shifted up from 170 to fit 32-tall */
#define FACE_COMP_MAX         1024    /* slack over the ~787 B real chunk */

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

/* A.21: KERNEL-exported video selector — magic constant, &_A000H low-WORD
 * IS the runtime selector value patched in by the loader. Pair with the
 * DispDib BEGIN flag (0x8000) to set 320x200x8 mode persistently and
 * write to the framebuffer directly, bypassing GDI StretchDIBits. See
 * reference_a000h_idiom.md and dispdib_test_a21_proven.c. */
extern WORD _A000H;
static BYTE __far *g_fb_a000 = NULL;   /* far ptr to A000:0000 video memory, set after BEGIN */
static BOOL        g_dva_active = FALSE;

/* DispDib forward decl + flag values (disasm-derived, S17). */
WORD FAR PASCAL DisplayDib(LPBITMAPINFO lpbi, LPSTR lpBits, WORD wFlags);
#define DD_MODE_320x200x8 0x0001
#define DD_NOPAL_OR_TASK  0x0040
#define DD_BEGIN          0x8000
#define DD_END            0x4000

/* VSWAP buffers. walls[] = 32 * 4096 = 128 KB exceeds the 64 KB segment
 * cap that __far enforces; use __huge so Watcom places it across multiple
 * segments. Each 4 KB row sits inside a single segment so DrawWallStripCol
 * can deref by row to far ptr (mirror sprites[] pattern from A.14). */
static BYTE  __huge walls[WALL_COUNT][4096];
/* sprites[] = 58 * 4096 = 237 KB on __huge (was 104 KB / 26 slots in
 * A.16a). Each per-sprite 4-KB row still fits a single segment so the
 * inner sample loop is near-pointer cost; only sprite_idx selects which
 * row's segment we deref into. */
static BYTE  __huge sprites[NUM_SPRITES][SPRITE_MAX];
static WORD  sprite_len[NUM_SPRITES];
static DWORD __far pageoffs[CHUNKS_MAX];
static WORD  __far pagelens[CHUNKS_MAX];

/* Sparse VSWAP-chunk-offset table: sprite_chunk_offs[slot] = chunk index
 * relative to sprite_start_idx. Lets us skip the 32 unused SPR_STAT_16..47
 * chunks while loading SPR_GRD_S/W frames at slots 18..57. The legacy
 * A.14 contiguous load (slots 0..17 -> chunks 0..17) is preserved as the
 * identity prefix of this table.
 *
 * Slot 58 (PISTOL_READY_SLOT) carries 0 here as a placeholder — the real
 * offset is `total_sprites - PISTOL_READY_OFFSET`, patched at runtime in
 * LoadVSwap() right after the header parse, before the load loop runs.
 * Patching is necessary because the player-attack frames are at the
 * trailing end of the sprite enum (positions vary slightly between WL1
 * shareware and WL6 / SOD / SDM full sets). */
static int sprite_chunk_offs[NUM_SPRITES] = {
    /* slots 0..17: SPR_DEMO, SPR_DEATHCAM, SPR_STAT_0..15 */
     0,  1,  2,  3,  4,  5,  6,  7,
     8,  9, 10, 11, 12, 13, 14, 15,
    16, 17,
    /* slots 18..25: SPR_GRD_S_1..S_8 (Wolf3D enum values 50..57) */
    50, 51, 52, 53, 54, 55, 56, 57,
    /* slots 26..33: SPR_GRD_W1_1..W1_8 (enum values 58..65) */
    58, 59, 60, 61, 62, 63, 64, 65,
    /* slots 34..41: SPR_GRD_W2_1..W2_8 (enum values 66..73) */
    66, 67, 68, 69, 70, 71, 72, 73,
    /* slots 42..49: SPR_GRD_W3_1..W3_8 (enum values 74..81) */
    74, 75, 76, 77, 78, 79, 80, 81,
    /* slots 50..57: SPR_GRD_W4_1..W4_8 (enum values 82..89) */
    82, 83, 84, 85, 86, 87, 88, 89,
    /* slot 58: SPR_PISTOLREADY (placeholder; patched in LoadVSwap) */
    0,
    /* slots 59..62: SPR_PISTOLATK1..4 (placeholders; patched in LoadVSwap) */
    0, 0, 0, 0,
    /* slots 63..65: SPR_GRD_DIE_1..3 (absolute enum 91..93, no patch) */
    91, 92, 93,
    /* slot 66: SPR_GRD_DEAD (absolute enum 95, no patch) */
    95,
    /* slots 67..69: SPR_GRD_SHOOT1..3 (absolute enum 96..98, no patch) */
    96, 97, 98,
    /* A.22 slots 70..77: pickup sprites SPR_STAT_24..33 (sparse, skip
     * 27/28 = bo_machinegun/chaingun which are weapon-class pickups
     * deferred to a later milestone). */
    26,    /* PK_FOOD_SLOT      */
    27,    /* PK_MEDKIT_SLOT    */
    28,    /* PK_CLIP_SLOT      */
    31,    /* PK_CROSS_SLOT     */
    32,    /* PK_CHALICE_SLOT   */
    33,    /* PK_BIBLE_SLOT     */
    34,    /* PK_CROWN_SLOT     */
    35     /* PK_FULLHEAL_SLOT  */
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
 *   tile_x, tile_y    : spawn tile (also painter-sort fast path; for
 *                       moving enemies, the live position is in
 *                       (x_q88, y_q88) and tile_x/tile_y track the cur
 *                       tile so collision/painter-sort still work).
 *   x_q88, y_q88      : sub-tile position in 1/256 tile units. Q24.8
 *                       so 64-tile map fits in 16 bits before fraction.
 *                       Decorations carry tile-center (tile*256+128).
 *   sprite_idx        : base slot for stand frames (decorations only).
 *   enemy_dir         : 0..7 dirtype (E/NE/N/NW/W/SW/S/SE), 0xFF = static
 *                       decoration. Mutated each AI tick when chasing.
 *   state             : OBJ_ST_STAND / OBJ_ST_WALK. Decoration = STAND
 *                       (never advanced).
 *   state_phase       : 0..3 walking phase. Stand = 0.
 *   state_tick_last   : GetTickCount of last AI tick, for time-scaled
 *                       phase + position advance. */
typedef struct {
    int   tile_x;
    int   tile_y;
    long  x_q88;
    long  y_q88;
    int   sprite_idx;     /* slot for DrawSpriteWorld; -1 = removed (e.g. pickup grabbed) */
    BYTE  enemy_dir;
    BYTE  state;
    BYTE  state_phase;
    DWORD state_tick_last;
    BYTE  hp;
    BYTE  pickup_kind;    /* A.22: PK_* enum, PK_NONE for decorations / enemies */
} Object;
#define OBJ_DIR_NONE  0xFF
#define OBJ_ST_STAND  0
#define OBJ_ST_WALK   1
#define OBJ_ST_DIE    2     /* A.18: 3-frame DIE_1..3 anim */
#define OBJ_ST_DEAD   3     /* A.18: frozen on SPR_GRD_DEAD, blocks LOS off */
#define OBJ_ST_SHOOT  4     /* A.20: 3-frame SHOOT1..3, T_Shoot fires on phase 1 */
#define GUARD_HP_INIT 25    /* vanilla WL_ACT2.C SpawnStand */
#define GUARD_DIE_FRAMES 3  /* DIE_1, DIE_2, DIE_3 */
#define GUARD_SCORE_VALUE 100L  /* vanilla GivePoints on guard kill */
static Object __far g_objects[MAX_OBJECTS];
static int          g_num_objects = 0;

/* A.18 game state. */
static BYTE  g_weapon_state = 0;     /* 0=READY, 1=FIRING */
static BYTE  g_weapon_phase = 0;     /* 0..3 ATK frame index when FIRING */
static DWORD g_weapon_tick_last = 0;
static BYTE  g_ammo  = 8;            /* vanilla initial pistol load */
static long  g_score = 0L;
static WORD  g_kills = 0;            /* telemetry only */

/* A.20 player health + death state. PoC: vanilla 0..100 HP scale. On
 * reaching 0, g_player_dead is set; PRIMARY tap triggers RestartLevel
 * which resets all gameplay state. PLAYER_LOW_HP threshold mirrors the
 * vanilla "low health = red digits" cue. */
#define PLAYER_HP_INIT   100
#define PLAYER_LOW_HP    25
static int  g_player_hp   = PLAYER_HP_INIT;
static BOOL g_player_dead = FALSE;

/* A.20.1 damage flash. DamagePlayer sets to 3 on hit. Each
 * InvalidatePlayerView decrements; red border is painted while > 1
 * so the final decremented tick is a clear-frame guarantee. WM_TIMER
 * forces moved=TRUE while > 0 so the flash sequence renders even when
 * the player is idle at the moment of damage. */
#define DAMAGE_FLASH_TICKS_INIT 3
#define DAMAGE_FLASH_BORDER_PX  5
#define DAMAGE_FLASH_COLOR      40   /* HUD_FG_LOW red */
static int g_damage_flash_ticks = 0;

/* A.19 minimap toggle (VK_HC1_F1). When TRUE the viewport rect is
 * repainted with a centered 64x64 minimap instead of the raycaster
 * scene. Default off so the player boots into the gameplay view. */
static BYTE  g_show_minimap = 0;
#define WEAPON_ST_READY   0
#define WEAPON_ST_FIRING  1
#define WEAPON_PHASE_MS   100        /* 100 ms per ATK frame -> 400 ms cycle */
#define WEAPON_FRAME_COUNT 4         /* PISTOLATK1..4 */
/* PRNG seed advanced cheaply on each FireWeapon for damage roll. We use
 * a tiny LCG that doesn't drag in the rand() runtime. */
static DWORD g_prng = 0x12345678UL;

/* 1D z-buffer: perpendicular distance Q8.8 per viewport column. Walls
 * write it; sprites read it and skip columns where the wall is closer. */
static long g_zbuffer[VIEW_W];
static WORD  chunks_in_file, sprite_start_idx, sound_start_idx;
static int   gVSwapErr = -1;

/* A.15.1 VGAGRAPH state. huff_b0/b1 split parallel arrays so a node
 * lookup is two indexed reads — slightly tighter than a struct {WORD
 * b0; WORD b1;} on 286 (no implicit shift to compute the sub-field
 * offset). face_pic[] is the deplaned 24x32 chunky bitmap ready for
 * a 1:1 byte copy in DrawFacePic. face_temp_planar[] holds the
 * 4-plane Huffman-expanded data only during init; reusing it for
 * compressed-data buffer would need a memcpy and saves 768 B BSS,
 * not worth the complication. */
static WORD  __far huff_b0[VGA_HUFF_NODES];
static WORD  __far huff_b1[VGA_HUFF_NODES];
static DWORD __far grstarts[VGA_HEAD_ENTRIES];
static BYTE  __far face_pic[FACE_BYTES];
static BYTE  __far face_temp_planar[FACE_BYTES];
static BYTE  __far face_comp[FACE_COMP_MAX];
static int   gVgaFaceErr = -1;

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

    /* Patch SPR_PISTOLREADY chunk offset at runtime: it sits at
     * (total_sprites - PISTOL_READY_OFFSET) in the WL1 sprite enum, where
     * total_sprites = sound_start_idx - sprite_start_idx. If the
     * computed offset is out of range we leave it as 0 — the load loop
     * will then notice the chunk is empty (or load garbage from chunk 0
     * = SPR_DEMO, which is benign as a fallback) and DrawWeaponOverlay
     * silently skips when sprite_len[PISTOL_READY_SLOT] == 0. */
    if (sound_start_idx > sprite_start_idx) {
        int total_sprites = (int)(sound_start_idx - sprite_start_idx);
        int pistol_off = total_sprites - PISTOL_READY_OFFSET;
        if (pistol_off > 0 && pistol_off < total_sprites) {
            sprite_chunk_offs[PISTOL_READY_SLOT] = pistol_off;
        }
        /* A.18: same end-of-VSWAP discovery for the four ATK frames.
         * Layout (WL_DEF.H:457-468 trailing weapon arsenal):
         *   total - 15 = SPR_PISTOLREADY
         *   total - 14 = SPR_PISTOLATK1
         *   total - 13 = SPR_PISTOLATK2
         *   total - 12 = SPR_PISTOLATK3
         *   total - 11 = SPR_PISTOLATK4
         * Each slot patched independently so a partial range still
         * gives best-effort; a missing chunk leaves slot at 0 and
         * DrawSpriteFixed silently no-ops on it. */
        {
            int atk_base = total_sprites - PISTOL_ATK1_OFFSET;
            int k;
            for (k = 0; k < 4; k++) {
                int off = atk_base + k;
                if (off > 0 && off < total_sprites) {
                    sprite_chunk_offs[PISTOL_ATK1_SLOT + k] = off;
                }
            }
        }
    }

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

/* ---- A.15.1 VGAGRAPH chunked Huffman pic loader ----
 *
 * Three-step boot pass:
 *   1. LoadVgaDict   : pull 1024 B Huffman dictionary into huff_b0/b1.
 *   2. LoadVgaHead   : pull 471 B chunk-offset table, decode 24-bit LE.
 *   3. LoadVgaFace   : seek VGAGRAPH to grstarts[FACE1A], read expanded
 *                       length WORD pair + Huffman payload, expand,
 *                       deplane to face_pic[].
 *
 * Sets gVgaFaceErr = 0 on full success; any failure leaves the field
 * negative (or a positive return code for diagnostics) and the static
 * BG bake falls back to DrawFacePlaceholder.
 *
 * HuffExpand uses standard EGADICT layout (head node = 254). For the
 * face pic the expanded length is small (768 B) so no >64K branch is
 * needed; the inner loop fits comfortably in one near segment. */

static int LoadVgaDict(void)
{
    HFILE f;
    OFSTRUCT of;
    BYTE __far buf[VGA_DICT_BYTES];
    UINT n;
    int  i;

    f = OpenFile("A:\\VGADICT.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;
    n = _lread(f, (LPVOID)buf, VGA_DICT_BYTES);
    _lclose(f);
    if (n != VGA_DICT_BYTES) return 2;

    /* 256 huffnodes, 4 B each: bit0:WORD LE, bit1:WORD LE. */
    for (i = 0; i < VGA_HUFF_NODES; i++) {
        huff_b0[i] = (WORD)buf[i*4 + 0] | ((WORD)buf[i*4 + 1] << 8);
        huff_b1[i] = (WORD)buf[i*4 + 2] | ((WORD)buf[i*4 + 3] << 8);
    }
    return 0;
}

static int LoadVgaHead(void)
{
    HFILE f;
    OFSTRUCT of;
    BYTE __far buf[VGA_HEAD_BYTES];
    UINT n;
    int  i;

    f = OpenFile("A:\\VGAHEAD.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 1;
    n = _lread(f, (LPVOID)buf, VGA_HEAD_BYTES);
    _lclose(f);
    if (n != VGA_HEAD_BYTES) return 2;

    /* 24-bit LE offsets, packed 3 bytes per entry. */
    for (i = 0; i < VGA_HEAD_ENTRIES; i++) {
        grstarts[i] = (DWORD)buf[i*3 + 0]
                    | ((DWORD)buf[i*3 + 1] << 8)
                    | ((DWORD)buf[i*3 + 2] << 16);
    }
    return 0;
}

/* Bit-streamed Huffman expander. Mirror of CAL_HuffExpand (id_ca.c
 * asm version) but in portable C. Reads a single bit per iteration
 * from src; on a literal byte (code < 256) writes to dst and resets
 * to head; on a node code (>= 256) follows to that node. */
static void HuffExpand(BYTE __far *src, BYTE __far *dst, WORD length)
{
    WORD node = VGA_HUFF_HEAD;
    WORD src_pos = 0;
    WORD dst_pos = 0;
    BYTE byte_val = src[0];
    BYTE bit = 1;

    src_pos = 1;
    while (dst_pos < length) {
        WORD code = (byte_val & bit) ? huff_b1[node] : huff_b0[node];
        bit <<= 1;
        if (bit == 0) {           /* rolled past bit 7 */
            byte_val = src[src_pos++];
            bit = 1;
        }
        if (code < 256) {
            dst[dst_pos++] = (BYTE)code;
            node = VGA_HUFF_HEAD;
        } else {
            node = (WORD)(code - 256);
        }
    }
}

/* Deplane a 4-plane chunky-pixel pic into a row-major linear bitmap.
 * Source layout per VL_MemToScreen:
 *   plane p stored at [p*plane_bytes .. (p+1)*plane_bytes - 1]
 *     row y of plane p starts at [p*plane_bytes + y*row_bytes_plane]
 *     col c (0..W/4-1) within that row holds pixel x = c*4 + p.
 * Result: dst[y*W + x] = pic byte at (x, y), 8-bit gamepal index. */
static void DeplanePic24x32(BYTE __far *src, BYTE __far *dst)
{
    int p, y, c;
    int src_off = 0;
    for (p = 0; p < 4; p++) {
        for (y = 0; y < FACE_H; y++) {
            for (c = 0; c < FACE_ROW_BYTES_PLANE; c++) {
                int x = c * 4 + p;
                dst[y * FACE_W + x] = src[src_off++];
            }
        }
    }
}

static int LoadVgaFace(void)
{
    HFILE  f;
    OFSTRUCT of;
    UINT   n;
    LONG   pos;
    DWORD  chunk_pos, chunk_size;
    DWORD  expanded_size;
    BYTE __far hdr[4];
    int    rc;

    rc = LoadVgaDict();
    if (rc != 0) return 100 + rc;
    rc = LoadVgaHead();
    if (rc != 0) return 200 + rc;

    if (VGA_FACE1A_CHUNK + 1 >= VGA_HEAD_ENTRIES) return 301;
    chunk_pos  = grstarts[VGA_FACE1A_CHUNK];
    chunk_size = grstarts[VGA_FACE1A_CHUNK + 1] - chunk_pos;
    if (chunk_size < 4 || chunk_size > FACE_COMP_MAX) return 302;

    f = OpenFile("A:\\VGAGRAPH.WL1", &of, OF_READ);
    if (f == HFILE_ERROR) return 303;

    pos = _llseek(f, (LONG)chunk_pos, 0);
    if (pos == -1L) { _lclose(f); return 304; }

    /* First 4 bytes of chunk = expanded length LE. */
    n = _lread(f, (LPVOID)hdr, 4);
    if (n != 4) { _lclose(f); return 305; }
    expanded_size = (DWORD)hdr[0]
                  | ((DWORD)hdr[1] << 8)
                  | ((DWORD)hdr[2] << 16)
                  | ((DWORD)hdr[3] << 24);
    if (expanded_size != FACE_BYTES) { _lclose(f); return 306; }

    /* Read remaining (chunk_size - 4) bytes of compressed data. */
    n = _lread(f, (LPVOID)face_comp, (UINT)(chunk_size - 4));
    _lclose(f);
    if (n != (UINT)(chunk_size - 4)) return 307;

    HuffExpand(face_comp, face_temp_planar, (WORD)FACE_BYTES);
    DeplanePic24x32(face_temp_planar, face_pic);
    return 0;
}

/* Forward decls: DrawFacePlaceholder + FB_Put both live further down
 * (kept near DrawHUD / framebuf primitives for organisational
 * symmetry). DrawFacePic needs them visible here. */
static void DrawFacePlaceholder(int x0, int y0);
static void FB_Put(int sx, int sy, BYTE pix);

/* 1:1 chunky blit of the deplaned face into framebuf at (x0, y0).
 * The face panel slot in the HUD (x=144..175, y=163..199) is wider
 * than the 24-px face — we left-anchor at x=148 (mirror placeholder).
 * If the loader failed we fall back to the primitive helmet so the
 * HUD never has an empty hole. */
static void DrawFacePic(int x0, int y0)
{
    int x, y;
    if (gVgaFaceErr != 0) {
        DrawFacePlaceholder(x0, y0);
        return;
    }
    for (y = 0; y < FACE_H; y++) {
        for (x = 0; x < FACE_W; x++) {
            FB_Put(x0 + x, y0 + y, face_pic[y * FACE_W + x]);
        }
    }
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

/* ---- A.16b enemy AI ticker (LOSCheck + AdvanceEnemies) ---- */

/* Map a Q10 angle to the dirtype (E=0/NE=1/N=2/NW=3/W=4/SW=5/S=6/SE=7)
 * whose dirangle_q10 is closest. Index = ((ang+64)>>7)&7 maps Q10
 * to a 0..7 CW-from-east ordinal; this LUT then folds it back to the
 * vanilla Wolf3D dirtype convention used by Object.enemy_dir. */
static const BYTE ord_to_dirtype[8] = { 0, 7, 6, 5, 4, 3, 2, 1 };

/* Unit-vector LUT per dirtype, in Q0.8 (256 = 1.0). Diagonals scaled
 * by 1/sqrt(2) ~= 181/256 so a guard moving NE doesn't cover sqrt(2)
 * tiles per tick at the same nominal speed as moving E. */
static const long enemy_dx_q8[8] = {  256,  181,    0, -181, -256, -181,    0,  181 };
static const long enemy_dy_q8[8] = {    0, -181, -256, -181,    0,  181,  256,  181 };

/* Tile-grid LOS via Bresenham. Walls always block; doors block when
 * mostly closed (mirror of IsBlockingForMove threshold). End tile is
 * NOT tested — that's where the player stands. Caller has already
 * verified |dx|+|dy|>0; identity case never enters the loop. */
static int LOSCheck(int ex, int ey, int px, int py)
{
    int dx = (px > ex) ? (px - ex) : (ex - px);
    int dy = (py > ey) ? (py - ey) : (ey - py);
    int sx = (px > ex) ? 1 : -1;
    int sy = (py > ey) ? 1 : -1;
    int err = dx - dy;
    int x = ex, y = ey;
    int e2;
    int ti;
    int safety = MAP_W + MAP_H;
    while (safety--) {
        if (x == px && y == py) return 1;
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
        if (x == px && y == py) return 1;
        if (IsWall(x, y)) return 0;
        if (IsDoor(x, y)) {
            ti = y * MAP_W + x;
            if (g_door_amt[ti] < DOOR_BLOCK_AMT) return 0;
        }
    }
    return 0;
}

/* Time-scaled enemy ticker. Mirrors AdvanceDoors pattern from A.13.1:
 * GetTickCount delta drives both walking-frame phase advance (250 ms
 * per phase => ~1 s W1->W4 cycle, close to vanilla path tic budget)
 * and sub-tile movement velocity (~0.5 tile/sec via ENEMY_SPEED_Q88).
 *
 * Behavior PoC (vanilla T_Stand + T_Chase fused, T_Path skipped):
 *  - If LOS to player AND dist > 1 tile: switch to WALK, snap dir to
 *    8-way toward player, advance position, advance phase.
 *  - Else: STAND, reset phase to 0 (idle pose front-still by sector).
 *  - Per-axis movement gate: if X-step would block, X is held while Y
 *    still attempts (matches the player-movement sticky-wall pattern
 *    from PollHeldKeys / ApplyHeldMovement). */
#define ENEMY_PHASE_MS         250L
#define ENEMY_SPEED_Q88        128L      /* Q8.8 per second => 0.5 tile/sec */
#define ENEMY_PAUSE_CLAMP_MS  1000L      /* alt-tab safety: cap dt to 1 s */

/* A.20: 20 vanilla tics @ 70 Hz tic rate = ~286 ms per shoot frame.
 * Three frames -> ~860 ms full anim. T_Shoot fires on phase 1 (vanilla
 * s_grdshoot2.action) so the visible "raise gun" frame precedes the
 * actual hitscan, matching the vanilla muzzle-flash beat. */
#define ENEMY_SHOOT_PHASE_MS   286L

/* Forward decls so AdvanceEnemies can call into the shoot subsystem. */
static void ShootPlayer(int idx, HWND hWnd);
static BYTE Prng8(void);    /* A.20: full-byte hit-roll RNG */

static DWORD g_enemy_last_tick = 0;

/* A.20: hWnd plumbed through so a SHOOT-state phase advance can route
 * the resulting DamagePlayer -> RedrawHUDHealth call without a global. */
static BOOL AdvanceEnemies(HWND hWnd)
{
    int   i;
    DWORD now, elapsed;
    long  dt_ms, step_q88;
    BOOL  changed = FALSE;
    int   player_tx, player_ty;
    long  dx_q88, dy_q88;
    int   etx, ety, ntx, nty;
    int   ang, ord;
    BYTE  new_dir;
    long  nx_q88, ny_q88;
    int   tic_equiv, sdist, schance;

    if (g_num_objects == 0) return FALSE;

    /* A.20: freeze all enemy AI ticks while the player is dead. The
     * killer enemy stands frozen on its last shoot frame — the death
     * tableau is informative ("who got me?") and the world stays still
     * until PRIMARY restarts. We still update g_enemy_last_tick so the
     * elapsed delta on resume isn't a giant catch-up burst. */
    if (g_player_dead) {
        g_enemy_last_tick = GetTickCount();
        return FALSE;
    }

    now = GetTickCount();
    if (g_enemy_last_tick == 0) {
        g_enemy_last_tick = now;
        return FALSE;
    }
    elapsed = now - g_enemy_last_tick;
    g_enemy_last_tick = now;
    if (elapsed == 0) return FALSE;
    if (elapsed > ENEMY_PAUSE_CLAMP_MS) elapsed = ENEMY_PAUSE_CLAMP_MS;
    dt_ms = (long)elapsed;
    step_q88 = (ENEMY_SPEED_Q88 * dt_ms) / 1000L;
    if (step_q88 < 1) step_q88 = 1;

    player_tx = (int)(g_px >> 8);
    player_ty = (int)(g_py >> 8);

    for (i = 0; i < g_num_objects; i++) {
        if (g_objects[i].enemy_dir == OBJ_DIR_NONE) continue;

        /* A.18: DEAD = frozen, no further state work. The corpse still
         * participates in painter sort (handled by DrawAllSprites). */
        if (g_objects[i].state == OBJ_ST_DEAD) continue;

        /* A.18: DIE = play 3 frames at ENEMY_PHASE_MS each, then freeze
         * on DEAD. No movement, no chase, no LOS — the enemy is on the
         * ground falling. */
        if (g_objects[i].state == OBJ_ST_DIE) {
            if ((now - g_objects[i].state_tick_last) >= (DWORD)ENEMY_PHASE_MS) {
                if (g_objects[i].state_phase + 1 >= GUARD_DIE_FRAMES) {
                    g_objects[i].state = OBJ_ST_DEAD;
                    g_objects[i].state_phase = 0;
                } else {
                    g_objects[i].state_phase++;
                }
                g_objects[i].state_tick_last = now;
                changed = TRUE;
            }
            continue;
        }

        /* A.20: SHOOT = 3-frame anim, T_Shoot fires on phase 1 (vanilla
         * s_grdshoot2.action). Movement is suppressed during the entire
         * anim — vanilla s_grdshoot* states have no think/move action,
         * the guard plants its feet to fire. After phase 2, return to
         * WALK so the chase logic re-evaluates next tick. */
        if (g_objects[i].state == OBJ_ST_SHOOT) {
            if ((now - g_objects[i].state_tick_last) >= (DWORD)ENEMY_SHOOT_PHASE_MS) {
                BYTE next_phase = (BYTE)(g_objects[i].state_phase + 1);
                if (next_phase == 1) {
                    /* Phase 0 -> 1 transition: T_Shoot. */
                    ShootPlayer(i, hWnd);
                }
                if (next_phase >= GUARD_SHOOT_FRAMES) {
                    g_objects[i].state = OBJ_ST_WALK;
                    g_objects[i].state_phase = 0;
                } else {
                    g_objects[i].state_phase = next_phase;
                }
                g_objects[i].state_tick_last = now;
                changed = TRUE;
            }
            continue;
        }

        etx = (int)(g_objects[i].x_q88 >> 8);
        ety = (int)(g_objects[i].y_q88 >> 8);

        /* Range gate: stand still when adjacent to (or on top of)
         * player tile. Beyond ~1.5 tiles + LOS pass = chase. */
        if (etx == player_tx && ety == player_ty) {
            g_objects[i].state = OBJ_ST_STAND;
            g_objects[i].state_phase = 0;
            continue;
        }
        if (!LOSCheck(etx, ety, player_tx, player_ty)) {
            g_objects[i].state = OBJ_ST_STAND;
            g_objects[i].state_phase = 0;
            continue;
        }

        dx_q88 = g_px - g_objects[i].x_q88;
        dy_q88 = g_py - g_objects[i].y_q88;
        ang = atan2_q10(dy_q88, dx_q88);
        ord = ((ang + 64) >> 7) & 7;
        new_dir = ord_to_dirtype[ord];
        g_objects[i].enemy_dir = new_dir;

        /* A.20: shoot trigger (vanilla T_Chase WL_ACT2.C:3079..3140).
         * LOS already passed above. dist = max(|dx_tile|,|dy_tile|).
         * dist <= 1 -> chance = 300 (always fires); else chance =
         * (tics<<4)/dist where tics = elapsed_ms/14 (70-Hz tic rate
         * equivalent). Roll Prng8 < chance. On trigger: state -> SHOOT,
         * phase = 0, no movement this tick. Suppressed while player is
         * dead so the world freezes during the death overlay. */
        if (!g_player_dead) {
            int dxt = (player_tx > etx) ? (player_tx - etx) : (etx - player_tx);
            int dyt = (player_ty > ety) ? (player_ty - ety) : (ety - player_ty);
            sdist = (dxt > dyt) ? dxt : dyt;
            tic_equiv = (int)(elapsed / 14L);
            if (tic_equiv < 1) tic_equiv = 1;
            if (sdist <= 1) schance = 300;
            else            schance = (tic_equiv << 4) / sdist;
            if ((int)Prng8() < schance) {
                g_objects[i].state = OBJ_ST_SHOOT;
                g_objects[i].state_phase = 0;
                g_objects[i].state_tick_last = now;
                changed = TRUE;
                continue;
            }
        }

        if (g_objects[i].state != OBJ_ST_WALK) {
            g_objects[i].state = OBJ_ST_WALK;
            g_objects[i].state_phase = 0;
            g_objects[i].state_tick_last = now;
        }

        if ((now - g_objects[i].state_tick_last) >= (DWORD)ENEMY_PHASE_MS) {
            g_objects[i].state_phase = (BYTE)((g_objects[i].state_phase + 1) & 3);
            g_objects[i].state_tick_last = now;
        }

        nx_q88 = g_objects[i].x_q88 + ((step_q88 * enemy_dx_q8[new_dir & 7]) >> 8);
        ny_q88 = g_objects[i].y_q88 + ((step_q88 * enemy_dy_q8[new_dir & 7]) >> 8);
        ntx = (int)(nx_q88 >> 8);
        nty = (int)(ny_q88 >> 8);

        /* Per-axis collision check, matching player-movement pattern. */
        if (!IsBlockingForMove(ntx, ety)) {
            g_objects[i].x_q88 = nx_q88;
            g_objects[i].tile_x = ntx;
            changed = TRUE;
        }
        if (!IsBlockingForMove(g_objects[i].tile_x, nty)) {
            g_objects[i].y_q88 = ny_q88;
            g_objects[i].tile_y = nty;
            changed = TRUE;
        }
    }
    return changed;
}

/* ---- A.18 weapon FSM + hitscan + damage ----
 *
 * Three pieces:
 *   AdvanceWeapon()  : time-scaled phase progression for the FIRING
 *                      state. Called from WM_TIMER beside AdvanceDoors
 *                      / AdvanceEnemies. Returns TRUE if the weapon
 *                      sprite changed (caller redraws viewport).
 *   FireWeapon(hWnd) : input handler. Gates on WEAPON_ST_READY +
 *                      g_ammo > 0; on accept decrements ammo, kicks
 *                      the FSM into FIRING phase 0, runs FireHitscan,
 *                      schedules HUD ammo redraw.
 *   FireHitscan(hWnd): walk g_objects[], find the closest non-dead
 *                      enemy whose projected screen span straddles
 *                      VIEW_W/2 (the crosshair column). Wall occludes
 *                      via g_zbuffer[VIEW_W/2]. Hit -> DamageEnemy.
 *
 * The PRNG is a tiny LCG so we don't drag rand()/srand() and pull in
 * Watcom's stdlib runtime. Damage roll = 5 + (high-7 bits & 7) =
 * 5..12, close to vanilla US_RndT()&7 + BASEDAMAGE for non-officer
 * guards. */

#define HITSCAN_NEAR_CULL_Q88 32L   /* same near-clip as DrawSpriteWorld */

static BYTE Prng7(void)
{
    g_prng = g_prng * 1664525UL + 1013904223UL;
    return (BYTE)((g_prng >> 24) & 0x7F);
}

/* A.20: full-byte PRNG variant for the vanilla 0..255 hit roll. */
static BYTE Prng8(void)
{
    g_prng = g_prng * 1664525UL + 1013904223UL;
    return (BYTE)(g_prng >> 24);
}

/* Forward decls so FireWeapon can call them without reordering. */
static void RedrawHUDAmmo(HWND hWnd);
static void RedrawHUDScore(HWND hWnd);
static void RedrawHUDHealth(HWND hWnd);   /* A.20 */
static void DamageEnemy(int idx, BYTE dmg, HWND hWnd);
static void DamagePlayer(int dmg, HWND hWnd);   /* A.20 */

static void FireHitscan(HWND hWnd)
{
    int  i;
    int  ca, sa;
    long wall_dist_q88;
    int  best_idx = -1;
    long best_camy = 0;

    if (g_num_objects == 0) return;

    /* Wall distance at crosshair column. g_zbuffer is populated by
     * the previous DrawViewport pass; fire is gated on a frame having
     * been rendered (always true at runtime — first WM_PAINT runs
     * before any input arrives). */
    wall_dist_q88 = g_zbuffer[VIEW_W / 2];
    best_camy = wall_dist_q88;

    ca = COS_Q15(g_pa);
    sa = SIN_Q15(g_pa);

    for (i = 0; i < g_num_objects; i++) {
        long rx, ry, cam_x, cam_y;
        long sprite_h_long;
        int  sprite_h, screen_x, dest_left, dest_right;

        if (g_objects[i].enemy_dir == OBJ_DIR_NONE) continue;     /* decoration */
        if (g_objects[i].state == OBJ_ST_DEAD) continue;          /* corpse */
        if (g_objects[i].state == OBJ_ST_DIE) continue;           /* falling, can't be re-hit */
        if (g_objects[i].hp == 0) continue;                       /* paranoia: live but zero hp */

        rx = g_objects[i].x_q88 - g_px;
        ry = g_objects[i].y_q88 - g_py;
        cam_y = ( rx * (long)ca + ry * (long)sa) >> 15;
        if (cam_y < HITSCAN_NEAR_CULL_Q88) continue;
        if (cam_y >= best_camy) continue;                         /* farther than current best or wall */

        cam_x = (-rx * (long)sa + ry * (long)ca) >> 15;

        /* Same projection math as DrawSpriteWorld. */
        screen_x      = VIEW_W/2 + (int)((cam_x * FOCAL_PIXELS) / cam_y);
        sprite_h_long = ((long)VIEW_H << 8) / cam_y;
        sprite_h      = (int)sprite_h_long;
        if (sprite_h < 1) continue;
        if (sprite_h > 4 * VIEW_H) sprite_h = 4 * VIEW_H;
        dest_left  = screen_x - sprite_h / 2;
        dest_right = dest_left + sprite_h;

        if (dest_left <= VIEW_W/2 && dest_right >= VIEW_W/2) {
            best_idx  = i;
            best_camy = cam_y;
        }
    }

    if (best_idx >= 0) {
        BYTE dmg = (BYTE)(5 + (Prng7() & 7));   /* 5..12 */
        DamageEnemy(best_idx, dmg, hWnd);
    }
}

static void DamageEnemy(int idx, BYTE dmg, HWND hWnd)
{
    if (idx < 0 || idx >= g_num_objects) return;
    if (g_objects[idx].enemy_dir == OBJ_DIR_NONE) return;
    if (g_objects[idx].state == OBJ_ST_DEAD) return;
    if (g_objects[idx].state == OBJ_ST_DIE)  return;

    if (dmg >= g_objects[idx].hp) {
        g_objects[idx].hp = 0;
        g_objects[idx].state = OBJ_ST_DIE;
        g_objects[idx].state_phase = 0;
        g_objects[idx].state_tick_last = GetTickCount();
        g_score += GUARD_SCORE_VALUE;
        g_kills++;
        RedrawHUDScore(hWnd);
    } else {
        g_objects[idx].hp = (BYTE)(g_objects[idx].hp - dmg);
        /* PoC: no PAIN flash. The enemy keeps walking; only the hp
         * counter ticks down toward the next fatal hit. Vanilla flips
         * to s_grdpain for one frame then re-enters chase — visually
         * minor at our 4-5 FPS, defer to A.19. */
    }
}

/* A.21: returns TRUE iff a shot actually fired (state advanced + ammo
 * spent). FALSE on rejected taps (mid-animation or out of ammo). The
 * caller uses this to decide whether to invalidate the viewport — an
 * accepted shot needs a redraw (ATK frame, hitscan flash, ammo HUD);
 * a rejected tap should cost zero per-frame work. Was void in A.18,
 * which made spam-at-zero-ammo trigger a full DrawViewport per WM_KEYDOWN
 * = ~50 ms wasted per spam press. */
static BOOL FireWeapon(HWND hWnd)
{
    if (g_weapon_state != WEAPON_ST_READY) return FALSE;
    if (g_ammo == 0) return FALSE;

    g_ammo--;
    g_weapon_state = WEAPON_ST_FIRING;
    g_weapon_phase = 0;
    g_weapon_tick_last = GetTickCount();

    FireHitscan(hWnd);
    RedrawHUDAmmo(hWnd);
    return TRUE;
}

/* ---- A.20 enemy fire-back + player damage ----
 *
 * Mirror of A.18 (player -> enemy hitscan) but with the direction
 * reversed: enemy at (etx, ety) shoots at player at (player_tx, player_ty).
 *
 * Hit roll (vanilla T_Shoot WL_ACT2.C:3444..3491):
 *   hitchance = 256 - dist*8       (player not running, not visible)
 *   if Prng8() < hitchance         hit
 *   damage = if dist<2  Prng8()>>2 (0..63)
 *            else if dist<4 Prng8()>>3 (0..31)
 *            else        Prng8()>>4 (0..15)
 *
 * PoC simplifications vs vanilla:
 *   - We don't track FL_VISABLE (the per-enemy "in player FOV" flag) so
 *     the visibility distinction (256 vs 160 hitchance) collapses to the
 *     "not visible" branch always. Slightly favours the player.
 *   - We don't track player thrustspeed; the run-vs-walk distinction is
 *     skipped. Player is treated as walking (lower hitchance branch).
 *   - LOSCheck is the same tile-grid Bresenham used for chase-trigger
 *     LOS. Vanilla CheckLine is sub-tile precision but the difference
 *     at our 4-5 FPS doesn't show.
 *
 * The shoot animation is driven by AdvanceEnemies' SHOOT-state branch,
 * which calls ShootPlayer on the phase 0 -> 1 transition (mirroring
 * vanilla s_grdshoot2.action = T_Shoot). ShootPlayer itself only does
 * the hit roll + damage, NOT the state transition.
 */
static void DamagePlayer(int dmg, HWND hWnd)
{
    if (g_player_dead) return;
    if (dmg <= 0) return;

    if (dmg >= g_player_hp) {
        g_player_hp = 0;
        g_player_dead = TRUE;
    } else {
        g_player_hp -= dmg;
    }
    RedrawHUDHealth(hWnd);
    /* A.20.1: kick the visual feedback flash. WM_TIMER will force
     * moved=TRUE while ticks > 0 so the next 3 timer ticks each run
     * InvalidatePlayerView -> red border painted on first 2 ticks,
     * cleared on tick 3 (border-paint condition is ticks > 1). */
    g_damage_flash_ticks = DAMAGE_FLASH_TICKS_INIT;
}

static void ShootPlayer(int idx, HWND hWnd)
{
    int  etx, ety, ptx, pty;
    int  dxt, dyt, dist;
    int  hitchance, damage;
    BYTE roll;

    if (g_player_dead) return;
    if (idx < 0 || idx >= g_num_objects) return;
    if (g_objects[idx].enemy_dir == OBJ_DIR_NONE) return;
    if (g_objects[idx].state != OBJ_ST_SHOOT) return;

    etx = (int)(g_objects[idx].x_q88 >> 8);
    ety = (int)(g_objects[idx].y_q88 >> 8);
    ptx = (int)(g_px >> 8);
    pty = (int)(g_py >> 8);

    /* Re-check LOS at fire time. Vanilla T_Shoot bails if CheckLine is
     * false — the player may have ducked behind a wall during the wind-up
     * frame. Fair gameplay: the visible "raise gun" frame is a tell. */
    if (!LOSCheck(etx, ety, ptx, pty)) return;

    dxt = (ptx > etx) ? (ptx - etx) : (etx - ptx);
    dyt = (pty > ety) ? (pty - ety) : (ety - pty);
    dist = (dxt > dyt) ? dxt : dyt;

    /* A.20.1: vanilla "visible" branch. LOSCheck passing means the
     * enemy can see the player AND vice versa (tile-grid LOS is
     * symmetric), which is exactly the vanilla FL_VISABLE condition
     * where vanilla halves the hitchance to model the player's dodge
     * window. A.20 used the harsher "not visible" formula (256-dist*8)
     * which was unfair given LOS already guaranteed visibility.
     * At dist=2: 128 (50%) vs A.20's 240 (94%). At dist=4: 96 (38%)
     * vs 224. At dist=6: 64 (25%) vs 208. */
    hitchance = 160 - dist * 16;
    if (hitchance < 0) hitchance = 0;

    roll = Prng8();
    if ((int)roll >= hitchance) return;     /* miss */

    /* Damage table from vanilla T_Shoot. */
    roll = Prng8();
    if      (dist < 2) damage = roll >> 2;  /* 0..63 close range */
    else if (dist < 4) damage = roll >> 3;  /* 0..31 mid */
    else               damage = roll >> 4;  /* 0..15 far */
    if (damage <= 0) return;                /* graze */

    DamagePlayer(damage, hWnd);
}

/* ---- A.22 pickups ----
 *
 * Vanilla GetBonus (WL_AGENT.C:667..789) maps each pickup variant to
 * a Give* call. We mirror exactly:
 *   bo_clip      +8 ammo  (gated ammo<99)
 *   bo_food      +10 hp   (gated hp<100)
 *   bo_firstaid  +25 hp   (gated hp<100)
 *   bo_fullheal  +99 hp + 25 ammo (no gate; 1up also vanilla gives
 *                an extra man + treasurecount++; we skip both since
 *                lives system not modeled).
 *   bo_cross     +100 score
 *   bo_chalice   +500 score
 *   bo_bible     +1000 score
 *   bo_crown     +5000 score
 *
 * TryGiveBonus returns FALSE on gate (caller leaves the pickup on the
 * floor). On TRUE, the caller marks the Object as removed by setting
 * sprite_idx = -1 — DrawSpriteWorld already skips negative idx via its
 * bounds check, painter sort can short-circuit with the same test.
 */
static BOOL TryGiveBonus(BYTE kind, HWND hWnd)
{
    int  add_hp = 0;
    int  add_ammo = 0;
    long add_score = 0;
    BOOL hud_health = FALSE, hud_ammo = FALSE, hud_score = FALSE;

    switch (kind) {
    case PK_AMMO_CLIP:
        if (g_ammo >= 99) return FALSE;
        add_ammo = 8;
        hud_ammo = TRUE;
        break;
    case PK_HEALTH_10:
        if (g_player_hp >= 100) return FALSE;
        add_hp = 10;
        hud_health = TRUE;
        break;
    case PK_HEALTH_25:
        if (g_player_hp >= 100) return FALSE;
        add_hp = 25;
        hud_health = TRUE;
        break;
    case PK_HEALTH_FULL:
        /* Vanilla 1up: +99 hp (full restore) + 25 ammo + extra man. We
         * skip the extra man (no lives) and grab unconditionally. */
        add_hp = 99;
        add_ammo = 25;
        hud_health = TRUE;
        hud_ammo = TRUE;
        break;
    case PK_TREAS_100:  add_score = 100L;  hud_score = TRUE; break;
    case PK_TREAS_500:  add_score = 500L;  hud_score = TRUE; break;
    case PK_TREAS_1K:   add_score = 1000L; hud_score = TRUE; break;
    case PK_TREAS_5K:   add_score = 5000L; hud_score = TRUE; break;
    default: return FALSE;
    }

    if (add_hp > 0) {
        g_player_hp += add_hp;
        if (g_player_hp > 100) g_player_hp = 100;
    }
    if (add_ammo > 0) {
        g_ammo = (BYTE)(g_ammo + add_ammo);
        if (g_ammo > 99) g_ammo = 99;
    }
    if (add_score > 0) {
        g_score += add_score;
    }
    if (hud_health) RedrawHUDHealth(hWnd);
    if (hud_ammo)   RedrawHUDAmmo(hWnd);
    if (hud_score)  RedrawHUDScore(hWnd);
    return TRUE;
}

/* CheckPickups: walk g_objects[] each WM_TIMER tick, harvest any pickup
 * sitting on the player's tile. Same-tile semantics mirror vanilla
 * (player->tilex == statobj->tilex && tiley == statobj->tiley).
 * Returns TRUE if any pickup was grabbed, so the caller can flag
 * moved=TRUE for the viewport redraw (the pickup sprite vanishes).
 *
 * Frozen while player is dead — pickups should not auto-grab during
 * the death overlay. */
static BOOL CheckPickups(HWND hWnd)
{
    int  i, ptx, pty;
    BOOL changed = FALSE;

    if (g_player_dead) return FALSE;
    if (g_num_objects == 0) return FALSE;

    ptx = (int)(g_px >> 8);
    pty = (int)(g_py >> 8);

    for (i = 0; i < g_num_objects; i++) {
        if (g_objects[i].pickup_kind == PK_NONE) continue;
        if (g_objects[i].sprite_idx < 0) continue;          /* already grabbed */
        if (g_objects[i].tile_x != ptx) continue;
        if (g_objects[i].tile_y != pty) continue;
        if (TryGiveBonus(g_objects[i].pickup_kind, hWnd)) {
            g_objects[i].sprite_idx = -1;                   /* removed */
            g_objects[i].pickup_kind = PK_NONE;             /* belt + braces */
            changed = TRUE;
        }
    }
    return changed;
}

/* Time-scaled phase progression. Called from WM_TIMER. Returns TRUE
 * if the weapon overlay sprite should re-blit (state or phase changed). */
static BOOL AdvanceWeapon(void)
{
    DWORD now;

    if (g_weapon_state != WEAPON_ST_FIRING) return FALSE;

    now = GetTickCount();
    if (g_weapon_tick_last == 0) {
        g_weapon_tick_last = now;
        return FALSE;
    }
    if ((now - g_weapon_tick_last) < (DWORD)WEAPON_PHASE_MS) return FALSE;

    g_weapon_tick_last = now;
    if (g_weapon_phase + 1 >= WEAPON_FRAME_COUNT) {
        g_weapon_state = WEAPON_ST_READY;
        g_weapon_phase = 0;
    } else {
        g_weapon_phase++;
    }
    return TRUE;
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
                g_objects[g_num_objects].tile_x         = tx;
                g_objects[g_num_objects].tile_y         = ty;
                g_objects[g_num_objects].x_q88          = ((long)tx << 8) | 0x80;
                g_objects[g_num_objects].y_q88          = ((long)ty << 8) | 0x80;
                g_objects[g_num_objects].sprite_idx     = sprite_idx;
                g_objects[g_num_objects].enemy_dir      = OBJ_DIR_NONE;
                g_objects[g_num_objects].state          = OBJ_ST_STAND;
                g_objects[g_num_objects].state_phase    = 0;
                g_objects[g_num_objects].state_tick_last= 0;
                g_objects[g_num_objects].hp             = 0;     /* decoration: not damageable */
                g_objects[g_num_objects].pickup_kind    = PK_NONE;
                g_num_objects++;
                g_num_static++;
                continue;
            }

            /* A.22 Branch 2a: pickup statics (food/medkit/clip/treasures/1up). */
            {
                BYTE  pk = PK_NONE;
                int   psl = -1;
                switch (obj) {
                case 47: pk = PK_HEALTH_10;   psl = PK_FOOD_SLOT;     break;
                case 48: pk = PK_HEALTH_25;   psl = PK_MEDKIT_SLOT;   break;
                case 49: pk = PK_AMMO_CLIP;   psl = PK_CLIP_SLOT;     break;
                case 52: pk = PK_TREAS_100;   psl = PK_CROSS_SLOT;    break;
                case 53: pk = PK_TREAS_500;   psl = PK_CHALICE_SLOT;  break;
                case 54: pk = PK_TREAS_1K;    psl = PK_BIBLE_SLOT;    break;
                case 55: pk = PK_TREAS_5K;    psl = PK_CROWN_SLOT;    break;
                case 56: pk = PK_HEALTH_FULL; psl = PK_FULLHEAL_SLOT; break;
                default: break;
                }
                if (psl >= 0) {
                    if (sprite_len[psl] == 0) continue;
                    g_objects[g_num_objects].tile_x         = tx;
                    g_objects[g_num_objects].tile_y         = ty;
                    g_objects[g_num_objects].x_q88          = ((long)tx << 8) | 0x80;
                    g_objects[g_num_objects].y_q88          = ((long)ty << 8) | 0x80;
                    g_objects[g_num_objects].sprite_idx     = psl;
                    g_objects[g_num_objects].enemy_dir      = OBJ_DIR_NONE;
                    g_objects[g_num_objects].state          = OBJ_ST_STAND;
                    g_objects[g_num_objects].state_phase    = 0;
                    g_objects[g_num_objects].state_tick_last= 0;
                    g_objects[g_num_objects].hp             = 0;
                    g_objects[g_num_objects].pickup_kind    = pk;
                    g_num_objects++;
                    g_num_static++;
                    continue;
                }
            }

            /* Branch 2: guard enemy tiles (stand or patrol, any tier).
             * Spawned in STAND state at tile center; AI ticker switches
             * them to WALK when LOS to player + dist > 1.5 tile. */
            dir4 = GuardTileToDir(obj);
            if (dir4 >= 0) {
                if (sprite_len[GUARD_S_FIRST_SLOT] == 0) continue;
                dir8 = (BYTE)(dir4 * 2);    /* WL_DEF.H dirtype: E/N/W/S = 0/2/4/6 */
                g_objects[g_num_objects].tile_x         = tx;
                g_objects[g_num_objects].tile_y         = ty;
                g_objects[g_num_objects].x_q88          = ((long)tx << 8) | 0x80;
                g_objects[g_num_objects].y_q88          = ((long)ty << 8) | 0x80;
                g_objects[g_num_objects].sprite_idx     = GUARD_S_FIRST_SLOT;
                g_objects[g_num_objects].enemy_dir      = dir8;
                g_objects[g_num_objects].state          = OBJ_ST_STAND;
                g_objects[g_num_objects].state_phase    = 0;
                g_objects[g_num_objects].state_tick_last= 0;
                g_objects[g_num_objects].hp             = GUARD_HP_INIT;
                g_objects[g_num_objects].pickup_kind    = PK_NONE;
                g_num_objects++;
                g_num_enemies++;
            }
        }
    }
}

/* SetupStaticBg / DrawViewport / DrawCrosshair / DrawMinimapView are
 * defined later in the file. Fwd decls so RestartLevel can call them. */
static void SetupStaticBg(void);
static void DrawViewport(void);
static void DrawCrosshair(void);
static void DrawMinimapView(void);

/* A.20: Reset all gameplay state to fresh-spawn values and force a full
 * viewport repaint. Triggered when the player taps PRIMARY while dead.
 *   - Player: hp -> 100, ammo -> 8, score/kills -> 0, weapon -> READY,
 *     position + heading -> spawn marker (InitPlayer).
 *   - Enemies + decorations: respawn from plane1 scan (ScanObjects).
 *     Per-enemy hp/state/phase reset to fresh GUARD_HP_INIT/STAND/0.
 *   - HUD: full re-bake into static_bg (the chrome doesn't change but
 *     SCORE/AMMO/HEALTH digit text needs the initial "100"/"08"/"000000"
 *     re-stamped; SetupStaticBg does both in one shot).
 *   - Doors: amt + dir tables left as-is. Closing-on-restart isn't worth
 *     the extra work for PoC — the player teleports back to spawn so any
 *     mid-anim door is far away. */
static void RestartLevel(HWND hWnd)
{
    g_player_hp    = PLAYER_HP_INIT;
    g_player_dead  = FALSE;
    g_ammo         = 8;
    g_score        = 0L;
    g_kills        = 0;
    g_weapon_state = WEAPON_ST_READY;
    g_weapon_phase = 0;
    g_weapon_tick_last = 0;
    g_enemy_last_tick  = 0;

    InitPlayer();
    ScanObjects();
    SetupStaticBg();

    /* Paint the viewport scene over the static_bg viewport region so
     * the WM_PAINT triggered below has finished framebuf content (HUD
     * chrome from SetupStaticBg + raycaster scene from DrawViewport).
     * Mirrors the WinMain init sequence. */
    if (g_show_minimap) {
        DrawMinimapView();
    } else {
        DrawViewport();
        DrawCrosshair();
    }

    /* Force a full repaint by invalidating the entire client area. The
     * normal InvalidatePlayerView only marks the viewport rect dirty;
     * after restart we want the HUD chrome refreshed too in case the
     * panel digits drifted (e.g. AMMO 00 in red -> 08 in white). */
    {
        RECT full;
        full.left = 0; full.top = 0;
        full.right = SCR_W; full.bottom = SCR_H;
        InvalidateRect(hWnd, &full, FALSE);
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

/* A.19: full-screen minimap mode. Fills the viewport rect with
 * MINIMAP_BG, then draws the 64x64 map centered. Same content as
 * the A.15.1 corner minimap (tile/object/door colors + player dot
 * + heading line) but at MINIMAP_VIEW_X0/Y0 instead of MINIMAP_X0/Y0.
 * Called only when g_show_minimap is TRUE; otherwise zero per-frame
 * cost (was the dominant H2 hot path at ~25-30 ms/frame). */
static void DrawMinimapView(void)
{
    int tx, ty;
    WORD tile, obj;
    BYTE wc, oc;
    int  ppx, ppy, hx, hy;

    /* Wipe the viewport rect with the minimap-mode background so the
     * raycaster scene from the previous frame doesn't bleed through. */
    FB_FillRect(VIEW_X0, VIEW_Y0, VIEW_W, VIEW_H, MINIMAP_BG);

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
            FB_Put(MINIMAP_VIEW_X0 + tx, MINIMAP_VIEW_Y0 + ty, oc ? oc : wc);
        }
    }

    /* Player position dot (3x3 cyan), heading line 4 px in cos/sin direction. */
    ppx = MINIMAP_VIEW_X0 + (int)(g_px >> 8);
    ppy = MINIMAP_VIEW_Y0 + (int)(g_py >> 8);
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
    long sy_acc, sy_step;
    int  tex_idx_clamped;
    int  vy0, vy1;
    int  ceil_top, ceil_bot;
    int  wall_top, wall_bot;
    int  floor_top, floor_bot;
    BYTE c;
    WORD __far *fbw;       /* A.19.2: WORD-aligned pair-write pointer */
    WORD pair_w;

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

    /* A.19.2: ceiling/wall/floor fills do WORD pair-writes. sx is even
     * (VIEW_X0=96 even + half-col cast advances col by 2), so fb1
     * is always WORD-aligned and (WORD __far *)fb1 is safe. */

    /* Ceiling fill. */
    fbw = (WORD __far *)(framebuf + (long)((SCR_H - 1) - ceil_top) * SCR_W + sx);
    for (dy = ceil_top; dy < ceil_bot; dy++) {
        *fbw = CEIL_PAIR;
        fbw -= (SCR_W >> 1);
    }

    /* Wall fill: sample texture, replicate byte to WORD pair, store. */
    fbw = (WORD __far *)(framebuf + (long)((SCR_H - 1) - wall_top) * SCR_W + sx);
    for (dy = wall_top; dy < wall_bot; dy++) {
        sy_src = (int)(sy_acc >> 16);
        if (sy_src > 63) sy_src = 63;
        c = texcol[sy_src];
        pair_w = (WORD)(((WORD)c << 8) | (WORD)c);
        *fbw = pair_w;
        fbw -= (SCR_W >> 1);
        sy_acc += sy_step;
    }

    /* Floor fill. */
    fbw = (WORD __far *)(framebuf + (long)((SCR_H - 1) - floor_top) * SCR_W + sx);
    for (dy = floor_top; dy < floor_bot; dy++) {
        *fbw = FLOOR_PAIR;
        fbw -= (SCR_W >> 1);
    }
}

/* Render one billboard. World position is tile center (Q8.8). Transform
 * to camera space by translating by -player_pos and rotating by -g_pa
 * (so player heading aligns with +cam_y / depth axis). Cull if behind or
 * very near. Project cam_x onto screen via constant focal=96 px. Sprite
 * height in pixels uses the same inverse-depth formula as walls so a
 * 64-tile-unit sprite covers the same vertical range as a wall at the
 * same depth. Per-column z-test against g_zbuffer makes walls occlude.
 *
 * A.19.1: inner pixel loop uses Q.16 step accumulator (mirror of
 * DrawWallStripCol). step_q16 = (64 << 16) / sprite_h is computed
 * once per sprite; srcx and sy_src derive from accumulators advanced
 * per dest_col / per pixel. Eliminates the per-pixel long division
 * that caused the close-enemy freeze (sprite_h >= ~256 -> 80 cols *
 * 256+ px * long div = 5+ M cycles on 286). */
static void DrawSpriteWorld(long obj_x_q88, long obj_y_q88, int sprite_idx)
{
    long rx, ry, cam_x, cam_y;
    int  ca, sa;
    int  screen_x, dest_left, dest_right, dest_col;
    int  col_iter_start, col_iter_end;
    int  sprite_h, dy_top, dy_start, dy_end, dy, srcx, sy, sx;
    long sprite_h_long;
    long step_q16, srcx_acc, sy_acc;
    int  dy_iter_start, dy_iter_end;
    int  view_y_top, view_y_bot;
    BYTE *sprite;
    BYTE __far *fb;
    WORD leftpix, rightpix, col_ofs, starty_src, endy_src, corr_top, src_idx;
    WORD far *dataofs;
    WORD far *post;

    if (sprite_idx < 0 || sprite_idx >= NUM_SPRITES) return;
    if (sprite_len[sprite_idx] == 0) return;

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

    /* A.19.1 once-per-sprite setup: Q.16 step + viewport y bounds. */
    step_q16   = (64L << 16) / (long)sprite_h;
    view_y_top = VIEW_Y0;
    view_y_bot = VIEW_Y0 + VIEW_H;

    /* A.19.2: pre-clip dest_col bounds to viewport [0, VIEW_W). Seed
     * srcx_acc so the first iter samples the same source column it did
     * in the unclipped loop. Eliminates ~hundreds of off-viewport
     * iterations per close sprite + the per-iter bound check. */
    col_iter_start = (dest_left  < 0)      ? 0      : dest_left;
    col_iter_end   = (dest_right > VIEW_W) ? VIEW_W : dest_right;
    if (col_iter_start >= col_iter_end) return;
    srcx_acc = (long)(col_iter_start - dest_left) * step_q16;

    for (dest_col = col_iter_start; dest_col < col_iter_end; dest_col++) {
        srcx = (int)(srcx_acc >> 16);
        srcx_acc += step_q16;

        /* Wall occlusion: skip column if a wall is in front. */
        if (g_zbuffer[dest_col] <= cam_y) continue;
        if (srcx < (int)leftpix || srcx > (int)rightpix) continue;
        col_ofs = dataofs[srcx - (int)leftpix];
        if (col_ofs >= SPRITE_MAX) continue;
        post = (WORD far *)(sprite + col_ofs);

        /* A.19.2: dest_col now bounded to [0, VIEW_W) by pre-clip, so
         * sx = VIEW_X0 + dest_col is in [VIEW_X0, VIEW_X0 + VIEW_W) =
         * [96, 224) which is always inside [0, SCR_W=320). Drop the
         * dead bound check. */
        sx = VIEW_X0 + dest_col;

        while (post[0] != 0) {
            endy_src   = (WORD)(post[0] >> 1);
            corr_top   = post[1];
            starty_src = (WORD)(post[2] >> 1);
            if (endy_src > 64 || starty_src > endy_src) break;

            /* Long mul ok: per post, not per pixel. */
            dy_start = dy_top + (int)(((long)starty_src * (long)sprite_h) / 64L);
            dy_end   = dy_top + (int)(((long)endy_src   * (long)sprite_h) / 64L);

            /* Pre-clip dy range to viewport once. Inner loop is
             * bound-check-free below. */
            dy_iter_start = (dy_start < view_y_top) ? view_y_top : dy_start;
            dy_iter_end   = (dy_end   > view_y_bot) ? view_y_bot : dy_end;
            if (dy_iter_start >= dy_iter_end) { post += 3; continue; }

            /* Init sy_acc so that at dy = dy_iter_start the extracted
             * sy = (dy_iter_start - dy_top) * 64 / sprite_h
             *    = (dy_iter_start - dy_top) * (step_q16 >> 16). */
            sy_acc = (long)(dy_iter_start - dy_top) * step_q16;

            /* Decrementing far pointer: bottom-up DIB so y++ -> fb-=SCR_W. */
            fb = framebuf
               + (long)((SCR_H - 1) - dy_iter_start) * (long)SCR_W
               + (long)sx;

            for (dy = dy_iter_start; dy < dy_iter_end; dy++) {
                sy = (int)(sy_acc >> 16);
                /* Q.16 rounding may push sy outside the post bounds.
                 * Defensive clamp; cost is 2 compares + 0..2 stores. */
                if (sy < (int)starty_src) sy = (int)starty_src;
                else if (sy >= (int)endy_src) sy = (int)endy_src - 1;
                src_idx = (WORD)(corr_top + (WORD)sy);
                if (src_idx < SPRITE_MAX) *fb = sprite[src_idx];
                fb     -= SCR_W;
                sy_acc += step_q16;
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
        /* A.22: skip removed pickups (sprite_idx = -1 after grab). */
        if (g_objects[i].sprite_idx < 0
            && g_objects[i].enemy_dir == OBJ_DIR_NONE) continue;
        obj_x_q88 = g_objects[i].x_q88;
        obj_y_q88 = g_objects[i].y_q88;
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

        /* Enemy rotation: pick the GRD_S or GRD_W frame that matches the
         * player's view angle relative to the enemy's facing. Static
         * decorations (enemy_dir == OBJ_DIR_NONE) use sprite_idx as-is.
         * A.18: DIE / DEAD frames are non-rotating (vanilla
         * statetype.rotate=false) so they bypass the atan2 path. */
        if (g_objects[idx].enemy_dir != OBJ_DIR_NONE) {
            if (g_objects[idx].state == OBJ_ST_DEAD) {
                sprite_idx = GRD_DEAD_SLOT;
            } else if (g_objects[idx].state == OBJ_ST_DIE) {
                int p = g_objects[idx].state_phase;
                if (p < 0) p = 0;
                if (p >= GUARD_DIE_FRAMES) p = GUARD_DIE_FRAMES - 1;
                sprite_idx = GRD_DIE1_SLOT + p;
            } else if (g_objects[idx].state == OBJ_ST_SHOOT) {
                /* A.20: SHOOT non-rotating (vanilla s_grdshoot*.rotate=
                 * false). The 3 SHOOT frames are forward-facing only —
                 * the guard turns to face you THEN raises the gun, which
                 * is why we don't sector-pick on this state. */
                int p = g_objects[idx].state_phase;
                if (p < 0) p = 0;
                if (p >= GUARD_SHOOT_FRAMES) p = GUARD_SHOOT_FRAMES - 1;
                sprite_idx = GRD_SHOOT1_SLOT + p;
            } else {
                long e2p_dx = g_px - g_objects[idx].x_q88;     /* Q8.8 enemy->player */
                long e2p_dy = g_py - g_objects[idx].y_q88;
                int  e2p_angle = atan2_q10(e2p_dy, e2p_dx);
                int  facing = dirangle_q10[g_objects[idx].enemy_dir & 7];
                /* Sign of (facing - e2p_angle) chosen so the SPR_GRD_S_n
                 * frame indices (CCW around the enemy in vanilla Wolf3D's
                 * art layout) match our CW-from-east Q10 angle convention.
                 * Verified empirically vs A.16a snap 0017/0019/0020. */
                int  rel = (facing - e2p_angle + 1024 + 64) & ANGLE_MASK;
                int  sector = (rel >> 7) & 7;          /* 0..7 of 128 each */
                if (g_objects[idx].state == OBJ_ST_WALK) {
                    /* slot = GUARD_W_FIRST_SLOT (26) + phase*8 + sector */
                    sprite_idx = GUARD_W_FIRST_SLOT
                               + ((int)g_objects[idx].state_phase << 3)
                               + sector;
                } else {
                    sprite_idx = GUARD_S_FIRST_SLOT + sector;
                }
            }
        }

        DrawSpriteWorld(g_objects[idx].x_q88,
                        g_objects[idx].y_q88,
                        sprite_idx);
    }
}

static void DrawWeaponOverlay(void);    /* fwd decl, defined later */

static void DrawViewport(void)
{
    int  col;
    int  ra, half_fov_a;
    int  tex_idx, tex_x;
    long dist_q88, perp_dist;

    /* Half-col cast: step=2, render col + col+1 with same data inside
     * DrawWallStripCol (paired writes). Halves cast cost + per-col setup.
     *
     * A.15.1 fix: nudge ra by 1 angle unit when it would land exactly on
     * a cardinal axis (E/N/W/S = 0/256/512/768 in our 1024-unit angle
     * domain). This only happens at the exact crosshair column
     * (col == VIEW_W/2 -> half_fov_a == 0) when g_pa is cardinal-aligned
     * — the DDA otherwise traces parallel to a grid axis and reports a
     * spurious near-zero hit at a tile corner, painting one bright wall
     * column through the viewport center. Cost: ~0.35 deg shift on a
     * single ray, visually imperceptible. Same fix benefits hitscan
     * reliability since FireHitscan reads g_zbuffer[VIEW_W/2]. */
    for (col = 0; col < VIEW_W; col += 2) {
        half_fov_a = ((col - VIEW_W/2) * FOV_ANGLES) / VIEW_W;
        ra = (g_pa + half_fov_a) & ANGLE_MASK;
        if ((ra & 0xFF) == 0) ra = (ra + 1) & ANGLE_MASK;
        dist_q88 = CastRay(ra, &tex_idx, &tex_x);
        /* Fish-eye correction: multiply by cos(half_fov_a). */
        perp_dist = (dist_q88 * (long)fov_correct[col]) >> 15;
        if (perp_dist < 16) perp_dist = 16;
        DrawWallStripCol(col, perp_dist, tex_idx, tex_x);
    }
    /* Sprites after walls: z-buffer is now populated, sprites read it. */
    DrawAllSprites();
    /* Weapon overlay last — the player's gun is foreground, paints over
     * any in-world sprite that overlaps the bottom-center of the viewport. */
    DrawWeaponOverlay();
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

/* Fixed-position 1:1 sprite blit for HUD/overlay use. Walks the
 * t_compshape post format (same as DrawSpriteWorld but no scale, no
 * z-buffer, no projection — just paint at (sx, sy) screen coords).
 * Sprite is 64x64 src; target framebuffer is 320x200 with bottom-up
 * orientation (FB_Put handles the y flip).
 *
 * Sprite-post format (t_compshape, see DrawSpriteWorld for the
 * world-space variant): bytes 0..3 = leftpix, rightpix WORDs; bytes
 * 4.. = WORD dataofs[rightpix-leftpix+1] giving per-column post-list
 * offsets; each post is a triple (endy*2, corr_top, starty*2) ending
 * at endy=0. Pixel data lives at corr_top + sy_src offset within the
 * sprite buffer. */
static void DrawSpriteFixed(int sx, int sy, int sprite_idx)
{
    BYTE *sprite;
    WORD leftpix, rightpix;
    WORD far *dataofs;
    WORD far *post;
    WORD col_ofs, starty_src, endy_src, corr_top, src_idx;
    int  srcx, sy_src;
    int  dst_x, dst_y, fb_y;

    if (sprite_idx < 0 || sprite_idx >= NUM_SPRITES) return;
    if (sprite_len[sprite_idx] == 0) return;

    sprite   = sprites[sprite_idx];
    leftpix  = *(WORD far *)(sprite + 0);
    rightpix = *(WORD far *)(sprite + 2);
    if (leftpix > 63 || rightpix > 63 || leftpix > rightpix) return;
    dataofs  = (WORD far *)(sprite + 4);

    for (srcx = (int)leftpix; srcx <= (int)rightpix; srcx++) {
        col_ofs = dataofs[srcx - (int)leftpix];
        if (col_ofs >= SPRITE_MAX) continue;
        post = (WORD far *)(sprite + col_ofs);

        dst_x = sx + srcx;
        if (dst_x < 0 || dst_x >= SCR_W) continue;

        while (post[0] != 0) {
            endy_src   = (WORD)(post[0] >> 1);
            corr_top   = post[1];
            starty_src = (WORD)(post[2] >> 1);
            if (endy_src > 64 || starty_src > endy_src) break;

            for (sy_src = (int)starty_src; sy_src < (int)endy_src; sy_src++) {
                dst_y = sy + sy_src;
                if (dst_y < 0 || dst_y >= SCR_H) continue;
                src_idx = (WORD)(corr_top + (WORD)sy_src);
                if (src_idx >= SPRITE_MAX) continue;
                fb_y = (SCR_H - 1) - dst_y;
                framebuf[(unsigned)fb_y * (unsigned)SCR_W + (unsigned)dst_x]
                    = sprite[src_idx];
            }
            post += 3;
        }
    }
}

/* A.17 weapon overlay: vanilla SPR_PISTOLREADY (64x64 t_compshape from
 * VSWAP) blitted 1:1 at viewport bottom-center. Sprite chunk index
 * was discovered at boot via `(sound_start_idx - sprite_start_idx) -
 * PISTOL_READY_OFFSET`, the WL1 shareware enum-tail layout
 * (WL_DEF.H:457-468 = KNIFE×5 + PISTOL×5 + MGUN×5 + CHAIN×5).
 *
 * Layout: 64 px wide × 64 px tall sprite, centered at viewport mid-x
 * (sx_left = 64 - 32 = 32), bottom flush at viewport y=162. Drawn last
 * in DrawViewport so it overpaints any in-world sprite that overlaps.
 *
 * If sprite_len[PISTOL_READY_SLOT] == 0 (chunk discovery failed or
 * VSWAP missing the player frames), DrawSpriteFixed silently returns
 * — the viewport just renders without the overlay rather than crash. */
static void DrawWeaponOverlay(void)
{
    int sx = VIEW_X0 + VIEW_W / 2 - 32;       /* 32 (pistol left edge) */
    int sy = VIEW_Y0 + VIEW_H     - 64;       /* 99 (pistol top) */
    int slot;
    if (g_weapon_state == WEAPON_ST_FIRING) {
        int p = g_weapon_phase;
        if (p < 0) p = 0;
        if (p >= WEAPON_FRAME_COUNT) p = WEAPON_FRAME_COUNT - 1;
        slot = PISTOL_ATK1_SLOT + p;
    } else {
        slot = PISTOL_READY_SLOT;
    }
    DrawSpriteFixed(sx, sy, slot);
}

/* HUD partial re-blit helpers. The static_bg HUD bake from A.15 only
 * carries the initial values (level=1, score=0, lives=3, health=100,
 * ammo=8). After fire/kill we update framebuf in-place over the digit
 * box and InvalidateRect the small region so WM_PAINT copies just
 * those pixels. The framebuf HUD region is otherwise untouched per
 * frame (DrawViewport writes only inside the viewport rect).
 *
 * Box geometry: width = digits * HUD_DIGIT_PITCH (9 or 29 px), one
 * extra pixel on the right covers the trailing digit's last column;
 * y starts at HUD_DIGIT_Y, height = HUD_DIGIT_H. We clear with HUD_BG
 * before redrawing the digits because DrawDigit only writes lit
 * pixels (transparent background). */
#define HUD_AMMO_X      243
#define HUD_AMMO_DIGITS 2
#define HUD_SCORE_X     57
#define HUD_SCORE_DIGITS 6
#define HUD_HEALTH_X    193        /* A.20: matches DrawHUD initial render */
#define HUD_HEALTH_DIGITS 3

static void RedrawHUDHealth(HWND hWnd)
{
    int  w = HUD_HEALTH_DIGITS * HUD_DIGIT_PITCH;
    RECT dirty;
    BYTE fg = (g_player_hp < PLAYER_LOW_HP) ? HUD_FG_LOW : HUD_FG;

    FB_FillRect(HUD_HEALTH_X, HUD_DIGIT_Y, w, HUD_DIGIT_H, HUD_BG);
    DrawNumber(HUD_HEALTH_X, HUD_DIGIT_Y, (long)g_player_hp, HUD_HEALTH_DIGITS, fg);

    dirty.left   = HUD_HEALTH_X;
    dirty.top    = HUD_DIGIT_Y;
    dirty.right  = HUD_HEALTH_X + w;
    dirty.bottom = HUD_DIGIT_Y + HUD_DIGIT_H;
    InvalidateRect(hWnd, &dirty, FALSE);
}

static void RedrawHUDAmmo(HWND hWnd)
{
    int  w = HUD_AMMO_DIGITS * HUD_DIGIT_PITCH;
    RECT dirty;
    BYTE fg = (g_ammo == 0) ? HUD_FG_LOW : HUD_FG;

    FB_FillRect(HUD_AMMO_X, HUD_DIGIT_Y, w, HUD_DIGIT_H, HUD_BG);
    DrawNumber(HUD_AMMO_X, HUD_DIGIT_Y, (long)g_ammo, HUD_AMMO_DIGITS, fg);

    dirty.left   = HUD_AMMO_X;
    dirty.top    = HUD_DIGIT_Y;
    dirty.right  = HUD_AMMO_X + w;
    dirty.bottom = HUD_DIGIT_Y + HUD_DIGIT_H;
    InvalidateRect(hWnd, &dirty, FALSE);
}

static void RedrawHUDScore(HWND hWnd)
{
    int  w = HUD_SCORE_DIGITS * HUD_DIGIT_PITCH;
    RECT dirty;

    FB_FillRect(HUD_SCORE_X, HUD_DIGIT_Y, w, HUD_DIGIT_H, HUD_BG);
    DrawNumber(HUD_SCORE_X, HUD_DIGIT_Y, g_score, HUD_SCORE_DIGITS, HUD_FG);

    dirty.left   = HUD_SCORE_X;
    dirty.top    = HUD_DIGIT_Y;
    dirty.right  = HUD_SCORE_X + w;
    dirty.bottom = HUD_DIGIT_Y + HUD_DIGIT_H;
    InvalidateRect(hWnd, &dirty, FALSE);
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
    /* FACE: A.15.1 = real BJ face from VGAGRAPH chunk 121 (24x32).
     * y shifted from 170 to 166 to fit 32-tall face inside HUD strip.
     * If the VGAGRAPH loader failed at boot, DrawFacePic falls back
     * to the A.15 primitive helmet so the panel never has a hole. */
    DrawFacePic(FACE_HUD_X, FACE_HUD_Y);
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
    /* A.21: debug bar removed — no longer useful now that perf is
     * shippable. The 500-ms heartbeat-aligned full-width InvalidateRect
     * was the residual freeze culprit (Windows merges its dirty rect
     * with InvalidatePlayerView's viewport-only rect into a near-full-
     * screen flush). Eliminated entirely. */
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
#define DEBUG_BAR_TICKS_INTERVAL  30  /* A.21 diag: was 10 (500 ms), bumped to 30 (1.5 s) to localize heartbeat-aligned freeze. */
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
    /* A.19: viewport-mode dispatch. Minimap-mode replaces the entire
     * raycaster pass with a single FillRect + 64x64 map blit. */
    if (g_show_minimap) {
        DrawMinimapView();
    } else {
        DrawViewport();
        DrawCrosshair();
        /* A.20.1: damage flash red border on top of viewport. Paint on
         * the first DAMAGE_FLASH_TICKS_INIT-1 ticks; the final tick
         * skips paint so DrawViewport's clean output is the last frame
         * the player sees (guarantees red is wiped even if player goes
         * idle when ticks transitions to 0). */
        if (g_damage_flash_ticks > 1) {
            int bw = DAMAGE_FLASH_BORDER_PX;
            BYTE rc = DAMAGE_FLASH_COLOR;
            FB_FillRect(VIEW_X0,                 VIEW_Y0,                  VIEW_W,         bw,                rc);
            FB_FillRect(VIEW_X0,                 VIEW_Y0 + VIEW_H - bw,    VIEW_W,         bw,                rc);
            FB_FillRect(VIEW_X0,                 VIEW_Y0 + bw,             bw,             VIEW_H - 2*bw,     rc);
            FB_FillRect(VIEW_X0 + VIEW_W - bw,   VIEW_Y0 + bw,             bw,             VIEW_H - 2*bw,     rc);
        }
        if (g_damage_flash_ticks > 0) g_damage_flash_ticks--;
    }
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

    /* A.19: dirty rect is the viewport rect only (the corner minimap
     * extension is gone). VIEW_X0=96, VIEW_W=128 -> x=96..223. */
    dirty.left   = VIEW_X0;
    dirty.top    = VIEW_Y0;
    dirty.right  = VIEW_X0 + VIEW_W;
    dirty.bottom = VIEW_Y0 + VIEW_H;
    InvalidateRect(hWnd, &dirty, FALSE);
    g_px_prev = g_px;
    g_py_prev = g_py;
    g_pa_prev = g_pa;
}

/* A.21: copy our DGROUP framebuf to A000:0000 video memory with the
 * bottom-up flip the renderer assumes. framebuf row 0 is the BOTTOM of
 * the screen (DIB convention preserved from A.2..A.19.2); A000:0000 row
 * 0 is the TOP. Per row: 320 bytes = 160 WORDs. _fmemcpy compiles to
 * REP MOVSW which is the canonical 286 fast block copy. Total cost:
 * 200 rows * ~640 cyc/row + ~30 cyc/setup = ~134k cyc/frame ≈ 11 ms,
 * vs ~40-60 ms for the StretchDIBits path it replaces. */
static void FlushFramebufToA000(int dy0, int dh)
{
    int y;
    BYTE __far *src;
    BYTE __far *dst;
    if (!g_dva_active || g_fb_a000 == NULL) return;
    if (dy0 < 0)             { dh += dy0; dy0 = 0; }
    if (dy0 + dh > SCR_H)    dh = SCR_H - dy0;
    if (dh <= 0) return;
    /* Walk top-down on dest, bottom-up on src so the row count matches. */
    for (y = 0; y < dh; y++) {
        dst = g_fb_a000 + (long)(dy0 + y) * SCR_W;
        src = (BYTE __far *)framebuf + (long)(SCR_H - 1 - (dy0 + y)) * SCR_W;
        _fmemcpy(dst, src, SCR_W);
    }
}

long FAR PASCAL _export WolfVisWndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC         hdc;

    switch (msg) {
    case WM_PAINT:
        /* A.21: BeginPaint/EndPaint still required so Windows clears the
         * dirty region tracking, but the blit goes direct to A000:0000.
         * Once g_dva_active is TRUE (DispDib BEGIN succeeded), GDI is
         * suspended for the duration; no palette select / no StretchDIBits.
         * If BEGIN failed (g_dva_active == FALSE), we silently skip the
         * blit — the screen will stay on the Win desktop with no rendering,
         * which is at least diagnosable. */
        hdc = BeginPaint(hWnd, &ps);
        FlushFramebufToA000(ps.rcPaint.top, ps.rcPaint.bottom - ps.rcPaint.top);
        EndPaint(hWnd, &ps);
        (void)hdc;
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
        /* A.20: while dead, only PRIMARY (restart) is meaningful. All
         * movement / door / minimap input is suppressed so the world
         * stays still until the player chooses to restart. */
        if (g_player_dead && wp != VK_HC1_PRIMARY) return 0;
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
            /* A.20: when the player is dead, PRIMARY restarts the level
             * (reset all gameplay state + repaint full client area). The
             * tap is consumed; FireWeapon is NOT called this press. The
             * dead state is exited inside RestartLevel; subsequent taps
             * fire normally.
             * A.18: PRIMARY is now FIRE. Door toggle moved to SECONDARY.
             * A.21 fix: only flag moved if FireWeapon actually fired —
             * spam-at-zero-ammo or mid-animation taps now cost ~0 per press
             * instead of triggering a full DrawViewport. */
            if (g_player_dead) {
                RestartLevel(hWnd);
            } else if (FireWeapon(hWnd)) {
                moved = TRUE;
            }
            break;
        case VK_HC1_SECONDARY:
            /* A.18: SECONDARY is now door toggle (A.14.1 -> A.18 swap).
             * Removed the legacy OPL click — it was a sanity check,
             * superseded by the music keys F1/F3. */
            if (ToggleDoorInFront()) moved = TRUE;
            break;
        case VK_HC1_F1:
            /* A.19: F1 (Xbox X / hand-controller third button) toggles
             * the minimap overlay. moved=TRUE forces InvalidatePlayerView
             * so the viewport rect is repainted with the new mode. The
             * A.10..A.15.1 music start binding here is dropped — OPL/IMF
             * subsystem stays in the binary but is no longer driven from
             * input (sqActive=FALSE = zero per-frame cost). */
            g_show_minimap = (BYTE)!g_show_minimap;
            moved = TRUE;
            break;
        case VK_HC1_F3:
            /* A.19: F3 reserved for a future strafe-toggle milestone.
             * Music stop binding (A.10..A.15.1) dropped per F1 note. */
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

        /* A.20: while dead, the world is frozen — no held-key movement,
         * no door anim, no enemy AI, no weapon FSM. Only PRIMARY in
         * WM_KEYDOWN can break the freeze (RestartLevel). The frozen
         * tableau (HEALTH=000 in red, killer guard mid-shoot frame) is
         * the death indicator. */
        if (g_player_dead) return 0;

        /* Async-poll the keyboard for held d-pad state, then apply
         * one tick of movement per held key. Async poll is the canonical
         * Win16 substitute for WM_KEYUP-driven release tracking. */
        PollHeldKeysFromAsync();
        moved = ApplyHeldMovement();
        /* Advance any door animations one step. Either a held-key
         * movement OR a door change requires a viewport redraw. */
        if (AdvanceDoors()) moved = TRUE;
        /* Advance enemy AI: phase-cycle walking sprites + sub-tile
         * movement when LOS+range trigger. State machine internal to
         * AdvanceEnemies; we just mark the viewport dirty if any enemy
         * changed pose or position. */
        if (AdvanceEnemies(hWnd)) moved = TRUE;
        /* A.22: same-tile pickup harvest. Returns TRUE if a pickup was
         * grabbed (sprite_idx -> -1), which needs a viewport redraw so
         * the item visually disappears. */
        if (CheckPickups(hWnd)) moved = TRUE;
        /* A.18: weapon FSM phase tick. AdvanceWeapon returns TRUE if
         * the FIRING phase advanced (incl. auto-return to READY) so
         * the overlay sprite changes; trigger a viewport redraw to
         * paint the new pistol frame. */
        if (AdvanceWeapon()) moved = TRUE;
        /* A.20.1: while the damage-flash counter is active, force a
         * viewport redraw each tick so the red border renders + clears
         * even if the player isn't moving when struck. */
        if (g_damage_flash_ticks > 0) moved = TRUE;
        if (moved) InvalidatePlayerView(hWnd);

        /* A.21: debug bar heartbeat block removed (was the residual
         * 500-ms freeze cause via dirty-rect merge with InvalidatePlayerView). */
        return 0;
    }

    case WM_SETFOCUS:   has_focus = TRUE;  return 0;
    case WM_KILLFOCUS:  has_focus = FALSE; return 0;

    /* A.21: WM_QUERYNEWPALETTE / WM_PALETTECHANGED / WM_CREATE GDI palette
     * select were needed when WM_PAINT did StretchDIBits with DIB_PAL_COLORS
     * — they coordinated with the system palette. Once we own the video
     * mode via DispDib BEGIN, those messages are not actionable for us. */

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

    gVgaFaceErr = LoadVgaFace();

    gAudioHdrErr = LoadAudioHeader();
    if (gAudioHdrErr == 0) {
        gMusicLoadErr = LoadMusicChunk(MUSIC_SMOKE_CHUNK);
        if (gMusicLoadErr == 0) OplInit();
    }

    InitPlayer();
    ScanObjects();

    SetupStaticBg();
    /* A.19: initial render mirrors InvalidatePlayerView dispatch.
     * g_show_minimap defaults to FALSE so we paint the centered
     * raycaster scene; F1 will swap in the minimap on demand. */
    if (g_show_minimap) {
        DrawMinimapView();
    } else {
        DrawViewport();
        DrawCrosshair();
    }

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
        wc.lpszClassName = "WolfVISa22";
        if (!RegisterClass(&wc)) return 0;
    }

    hWnd = CreateWindow(
        "WolfVISa22", "WolfVISa22",
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

    /* A.21: take over the video mode and grab the A000 framebuffer.
     * BEGIN sets 320x200x8 with our 256-color palette (loaded from bmi),
     * cursor disappears, GDI is suspended for the duration. The selector
     * idiom (low WORD of &_A000H far ptr) is the offset-of-symbol patched
     * by the loader to the runtime selector — see reference_a000h_idiom.md.
     * If g_dva_active stays FALSE here, the renderer falls back to no-blit
     * (silent black screen) which is at least diagnosable. */
    if (DisplayDib((LPBITMAPINFO)&bmi, NULL,
                   DD_BEGIN | DD_MODE_320x200x8 | DD_NOPAL_OR_TASK) == 0) {
        WORD sel = (WORD)((DWORD)(LPVOID)&_A000H);
        if (sel != 0) {
            g_fb_a000   = (BYTE __far *)(((DWORD)sel) << 16);
            g_dva_active = TRUE;
        }
    }

    /* Force one full-screen flush so the first frame is visible without
     * waiting for any per-frame InvalidateRect to fire. */
    if (g_dva_active) FlushFramebufToA000(0, SCR_H);

    /* 50 ms = 20 Hz — fast enough for held-key movement to feel
     * continuous. Debug bar throttles itself to 1 Hz inside WM_TIMER. */
    SetTimer(hWnd, 1, MOVE_POLL_MS, NULL);

    for (;;) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                /* A.21: restore the desktop before returning. END
                 * decrements [0x44e] and runs the GDI/palette restore
                 * helper. Skip if BEGIN never succeeded — ENDing without
                 * a matching BEGIN is a no-op per disasm of helper 0x8a5
                 * but we don't risk it. */
                if (g_dva_active) {
                    DisplayDib((LPBITMAPINFO)&bmi, NULL,
                               DD_END | DD_MODE_320x200x8 | DD_NOPAL_OR_TASK);
                    g_dva_active = FALSE;
                    g_fb_a000    = NULL;
                }
                return (int)msg.wParam;
            }
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
