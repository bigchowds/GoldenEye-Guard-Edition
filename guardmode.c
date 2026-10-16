#include <ultra64.h>
#include <math.h>
#include <limits.h>
#include <boss.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <random.h>
#include "bondaicommands.h"
#include "guardmode.h"
#include "bondview.h"
#include "bondinv.h"
#include "player.h"
#include "chr.h"
#include "chraction.h"
#include "chrobjdata.h"
#include "gun.h"
#include "lv.h"
#include "propobj.h"
#include "stan.h"
#include "model.h"
#include "math_atan2f.h"
#include "objective_status.h"
#include "mp_music.h"
#include "loadobjectmodel.h"
#include "chrai.h"
#include "matrixmath.h"
#include "front.h"
#include <music.h>
#include <snd.h>

extern bool g_isBondKIA;
extern s32 objectiveregisters1;
extern enum CAMERAMODE g_CameraMode;
extern void mp_respawn_handler(void);
extern void bondviewUpdatePlayerCollisionBounds(void);
extern s32 getPropForHeldItem(ITEM_IDS item);
extern void chrStopFiring(ChrRecord *chr);

GuardModeState g_GuardMode = {0};
ChrRecord *g_GuardModeShooter = NULL;
static s32 guardids[10] = {20, 21, 23, 24, 25, 26, 27, 28, 29, 30};
static s32 taken[10];
static PropRecord *lootweapons[10];
static s32 nextdecision[11];
static s32 allyengaged[10];
static s32 allynextshot[10];
static s32 allyburst[10];
static s32 routecommand = -1;
static s32 navigationuntil = 0;
static s32 progressat = 0;
static coord3d progresspos;
static s32 nextsidestep = 0;
static s32 stepside = 0;
static s32 lootdeadline = 0;
static s32 tankat = 0;
static s32 tanknextshot = 0;
static s32 tankguardnextshot = 0;
static s32 tankhandgunfiring = FALSE;
static s32 tankhandguntarget = -2;
static s32 tanklastframe = -1;
static f32 tankyaw = 0.0f;
static f32 tankspeed = 0.0f;
static f32 tankheight = 0.0f;
static s32 tanksalvos[4];
static coord3d tankfirepos[4];
static ALSoundState *tankengine = NULL;
/* Stay west of the escape aircraft's wing and missile-battery wreck. */
static s32 tankpath[10] = {143, 152, 151, 150, 149, 160, 159, 158, 157, 166};
static s32 objectivetags[4] = {1, 0, 4, 5};
static s32 guardmodeRoutePad(void);
static void guardmodeObjectiveMaskTick(void);
static s32 guardmodeVehicleControl(ChrRecord *chr);
extern coord3d g_TankModelPositionOffset;
extern s32 bondviewCallTankCollisionStatus(coord3d *pos, StandTile *stan, f32 yaw);
extern f32 g_TankShellSpeed;
extern struct firing_anim_struct firing_animation_groups[][6];
extern void chrlvFireWeaponRelated(ChrRecord *chr, s32 hand);
extern void bondviewGetTankCollisionBounds(rect4f *bounds, coord3d *pos, f32 yaw);
/* Door approach, vestibule, inner doorway, desk, back out, then aircraft.
 * Retail Runway's AI waypoint graph has no nodes inside this building. */
static s32 routesteps[13] = {112, 177, 178, 179, 176,
    179, 178, 177, 112, 144, 146, 156, 10002};
extern stagesetup g_CurrentSetup;
extern void chrlvInitActAttackWalk(ChrRecord *chr, s32 speed);
extern s32 sub_GAME_7F02A1E8(ChrRecord *chr, GUNHAND side, f32 distance);
extern void chrlvSideStepAnimationRelated(ChrRecord *chr, GUNHAND side);

static s32 guardmodeSlot(s32 chrid)
{
    s32 i;
    for (i = 0; i < 10; i++) if (guardids[i] == chrid) return i;
    return -1;
}

s32 guardmodeActive(void)
{
    return g_GuardMode.active && gamemode != GAMEMODE_MULTI
        && bossGetStageNum() == LEVELID_RUNWAY;
}

void guardmodeReset(s32 stage)
{
    s32 i;
    GuardModeState clear = {0};
    g_GuardMode = clear;
    for (i = 0; i < 4; i++) tanksalvos[i] = 0;
    g_GuardMode.active = stage == LEVELID_RUNWAY && gamemode != GAMEMODE_MULTI;
    g_GuardMode.currentguard = -1;
    g_GuardMode.body = BODY_Siberian_Guard_2;
    g_GuardMode.head = HEAD_Male_Jim;
    g_GuardMode.deathat = -1;
    g_GuardMode.finishat = -1;
    g_GuardMode.lastframe = -1;
    g_GuardMode.targetid = -2;
    g_GuardMode.retaliateid = -2;
    g_GuardMode.tankguardtarget = -2;
    g_GuardMode.tanklastguard = -2;
    g_GuardMode.lastpainsfx = -1;
    g_GuardMode.clip = 30;
    g_GuardModeShooter = NULL;
    routecommand = -1;
    navigationuntil = 0;
    progressat = 0;
    nextsidestep = 0;
    stepside = 0;
    lootdeadline = 0;
    tankat = 0;
    tanknextshot = 0;
    tankguardnextshot = 0;
    tankhandgunfiring = FALSE;
    tankhandguntarget = -2;
    tanklastframe = -1;
    tankyaw = 0.0f;
    tankspeed = 0.0f;
    tankengine = NULL;
    for (i = 0; i < 10; i++)
    {
        taken[i] = FALSE;
        lootweapons[i] = NULL;
        allyengaged[i] = FALSE;
        allynextshot[i] = 0;
        allyburst[i] = 0;
    }
    for (i = 0; i < 11; i++) nextdecision[i] = 0;
}

static s32 guardmodeAlive(ChrRecord *chr)
{
    return chr != NULL && chr->model != NULL && chr->prop != NULL
        && !(chr->hidden & CHRHIDDEN_REMOVE)
        && !(chr->chrflags & CHRFLAG_HIDDEN)
        && !chrIsDead(chr);
}

static f32 guardmodeDistance2(PropRecord *a, PropRecord *b)
{
    f32 x = a->pos.x - b->pos.x;
    f32 y = a->pos.y - b->pos.y;
    f32 z = a->pos.z - b->pos.z;
    return x*x + y*y + z*z;
}

static f32 guardmodeHorizontalDistance2(coord3d *a, coord3d *b)
{
    f32 x = a->x - b->x;
    f32 z = a->z - b->z;
    return x*x + z*z;
}

s32 guardmodeIsBond(ChrRecord *chr)
{
    return guardmodeActive() && chr != NULL && chr == g_GuardMode.bond;
}

s32 guardmodeBondCanStagger(ChrRecord *chr)
{
    return guardmodeIsBond(chr) && chr->damage < chr->maxdamage
        && chr->maxdamage - chr->damage <= chr->maxdamage * 0.15f
        && !guardmodeBondInTank(chr) && g_GuardMode.tankphase != 2;
}

static s32 guardmodePlayerTargetable(void)
{
    return !g_CurrentPlayer->bonddead && g_GuardMode.deathat < 0
        && g_CameraMode == CAMERAMODE_FP;
}

PropRecord *guardmodeDroneTarget(void)
{
    if (!guardmodeActive() || !g_GuardMode.initialized || !g_GuardMode.combatstarted
        || g_GuardMode.result != 0 || !guardmodeAlive(g_GuardMode.bond)) return NULL;
    return g_GuardMode.bond->prop;
}

void guardmodeDroneHit(coord3d *origin)
{
    f32 before;
    if (guardmodeDroneTarget() == NULL) return;
    before = g_GuardMode.bond->damage;
    handles_shot_actors(g_GuardMode.bond, HIT_GENERAL, origin, ITEM_FNP90, FALSE);
    if (g_GuardMode.bond->damage > before) g_GuardMode.dronehits++;
}

f32 guardmodePlayerDamageScale(void)
{
    return guardmodeActive() && g_GuardMode.difficulty >= DIFFICULTY_00 ? 1.5f : 1.0f;
}

s32 guardmodePlayerHitSound(void)
{
    /* The same 25 consecutive male guard pain samples used by
     * play_sound_for_shot_actor(), including the final lethal hit. */
    s32 sound = GET_HIT_MALE0_SFX + g_GuardMode.playerhurtsounds % 25;
    g_GuardMode.playerhurtsounds++;
    g_GuardMode.lastpainsfx = sound;
    return sound;
}

void guardmodeRecordDamage(ChrRecord *victim, f32 damage)
{
    s32 slot;
    if (!guardmodeIsBond(victim) || damage <= 0.0f || g_GuardModeShooter == NULL) return;
    slot = guardmodeSlot(g_GuardModeShooter->chrnum);
    if (slot < 0) return;
    g_GuardMode.guardhits[slot]++;
    g_GuardMode.guarddamage[slot] += damage;
}

/* Only a real bullet impact on the occupied vehicle reaches the driver.
 * The hull halves damage; walls, misses and an empty tank do not hurt Bond. */
s32 guardmodeTankHit(ObjectRecord *obj, s32 weapon, coord3d *origin, s32 playerhit)
{
    f32 before;
    if (!guardmodeBondInTank(g_GuardMode.bond) || obj != (ObjectRecord *)g_GuardMode.tank)
        return FALSE;
    before = g_GuardMode.bond->damage;
    handles_shot_actors(g_GuardMode.bond, HIT_GENERALHALF, origin, weapon, playerhit);
    if (g_GuardMode.bond->damage > before) g_GuardMode.tankbullethits++;
    return TRUE;
}

PropRecord *guardmodeTarget(ChrRecord *chr)
{
    ChrRecord *target;
    s32 slot;
    if (!guardmodeActive() || chr == NULL) return NULL;
    if (!guardmodeIsBond(chr))
    {
        slot = guardmodeSlot(chr->chrnum);
        if (slot < 0 || taken[slot] || !allyengaged[slot] || !guardmodeAlive(chr)
            || !guardmodeAlive(g_GuardMode.bond)) return NULL;
        return g_GuardMode.bond->prop;
    }
    if (g_GuardMode.targetid == -1)
        return guardmodePlayerTargetable() ? g_CurrentPlayer->prop : NULL;
    if (g_GuardMode.targetid < 0) return NULL;
    target = chrFindByLiteralId(g_GuardMode.targetid);
    return guardmodeAlive(target) ? target->prop : NULL;
}

void guardmodeAttackTarget(ChrRecord *chr, u32 *type, s32 *id)
{
    if (guardmodeTarget(chr) != NULL)
    {
        *type = guardmodeIsBond(chr) && g_GuardMode.targetid == -1 ? TARGET_BOND : TARGET_CHR;
        *id = guardmodeIsBond(chr)
            ? (g_GuardMode.targetid == -1 ? 0 : g_GuardMode.targetid) : GUARDMODE_BOND_ID;
    }
}

static void guardmodeBodyPoint(PropRecord *target, coord3d *point, s32 head)
{
    *point = target->pos;
    if (target == g_CurrentPlayer->prop)
        point->y -= bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer) * (head ? 0.02f : 0.4f);
    else
        point->y = target->chr->ground + target->chr->chrheight * (head ? 0.88f : 0.6f);
}

static s32 guardmodeClearPoint(ChrRecord *chr, PropRecord *target, coord3d *origin, coord3d *point)
{
    StandTile *stan = chr->prop->stan;
    u32 tankflags = 0;
    s32 occupied = target == g_GuardMode.bond->prop && guardmodeBondInTank(g_GuardMode.bond);
    s32 clear;
    if (stan == NULL || target->stan == NULL) return FALSE;
    /* Sight ends at Bond inside the hull. Ignore just his own vehicle for
     * this sight query; the subsequent native bullet trace still hits it. */
    if (occupied)
    {
        tankflags = g_GuardMode.tank->flags;
        g_GuardMode.tank->flags |= PROPFLAG_04000000;
    }
    clear = stanTestLineUnobstructed(&stan, origin->x, origin->z, point->x, point->z,
        CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE,
        origin->y, origin->y, point->y, point->y) && stan == target->stan;
    if (occupied) g_GuardMode.tank->flags = tankflags;
    return clear;
}

static s32 guardmodeExposedPoint(ChrRecord *chr, PropRecord *target, coord3d *origin, coord3d *point)
{
    guardmodeBodyPoint(target, point, FALSE);
    if (guardmodeClearPoint(chr, target, origin, point)) return TRUE;
    guardmodeBodyPoint(target, point, TRUE);
    return guardmodeClearPoint(chr, target, origin, point);
}

s32 guardmodeAimDirection(ChrRecord *chr, coord3d *origin, coord3d *direction)
{
    PropRecord *target = guardmodeTarget(chr);
    coord3d point;
    f32 length;
    f32 horizontal;
    f32 spread;
    f32 dx;
    f32 dz;
    if (target == NULL) return FALSE;
    if (!guardmodeExposedPoint(chr, target, origin, &point)) return FALSE;
    direction->x = point.x - origin->x;
    direction->y = point.y - origin->y;
    direction->z = point.z - origin->z;
    length = sqrtf(direction->x * direction->x + direction->y * direction->y
        + direction->z * direction->z);
    if (length < 0.01f) return FALSE;
    /* Strong but forgiving aim: real misses, including at medium range. */
    dx = direction->x;
    dz = direction->z;
    horizontal = sqrtf(dx * dx + dz * dz);
    spread = (((f32)(u32)randomGetNext() * (1.0f / UINT_MAX)) * 2.0f - 1.0f)
        * (guardmodeIsBond(chr) ? 0.04f : 0.06f) * length;
    if (horizontal > 0.01f)
    {
        direction->x += dz / horizontal * spread;
        direction->z -= dx / horizontal * spread;
    }
    direction->y += (((f32)(u32)randomGetNext() * (1.0f / UINT_MAX)) * 2.0f - 1.0f)
        * (guardmodeIsBond(chr) ? 0.023f : 0.03f) * length;
    length = sqrtf(direction->x * direction->x + direction->y * direction->y
        + direction->z * direction->z);
    direction->x /= length;
    direction->y /= length;
    direction->z /= length;
    return TRUE;
}

s32 guardmodePlayerInBulletPath(coord3d *origin, coord3d *direction)
{
    f32 dx = g_CurrentPlayer->prop->pos.x - origin->x;
    f32 dz = g_CurrentPlayer->prop->pos.z - origin->z;
    f32 horizontal = direction->x * direction->x + direction->z * direction->z;
    f32 along;
    f32 y;
    f32 radius = g_CurrentPlayer->field_488.collision_radius;
    if (!guardmodePlayerTargetable()) return FALSE;
    if (horizontal < 0.0001f) return FALSE;
    along = (dx * direction->x + dz * direction->z) / horizontal;
    if (along < 0.0f) return FALSE;
    dx -= along * direction->x;
    dz -= along * direction->z;
    y = origin->y + along * direction->y;
    return dx * dx + dz * dz <= radius * radius
        && y >= g_CurrentPlayer->stanHeight + 20.0f
        && y <= g_CurrentPlayer->stanHeight + bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer) + 10.0f;
}

void guardmodeNotifyHit(ChrRecord *chr, s32 playerhit)
{
    s32 slot;
    if (!guardmodeActive() || chrIsDead(chr)) return;
    if (!guardmodeIsBond(chr))
    {
        slot = guardmodeSlot(chr->chrnum);
        if (slot >= 0 && g_GuardModeShooter == g_GuardMode.bond)
        {
            g_GuardMode.guardalerts[slot] = g_GlobalTimer + 240;
            chr->seen_bond_time = g_GlobalTimer;
            nextdecision[slot] = 0;
        }
        return;
    }
    if (playerhit && guardmodePlayerTargetable()) g_GuardMode.retaliateid = -1;
    else if (g_GuardModeShooter != NULL && guardmodeAlive(g_GuardModeShooter))
        g_GuardMode.retaliateid = g_GuardModeShooter->chrnum;
    else return;
    g_GuardMode.retaliateuntil = g_GlobalTimer + 240;
    g_GuardMode.retaliations++;
    g_GuardMode.targetid = g_GuardMode.retaliateid;
    g_GuardMode.targetuntil = g_GuardMode.retaliateuntil;
    chr->seen_bond_time = g_GlobalTimer;
    nextdecision[10] = 0;
}

coord3d *guardmodeMoveTarget(ChrRecord *chr)
{
    s32 pad;
    if (!guardmodeIsBond(chr) || g_GuardMode.route >= 13) return NULL;
    if (g_GuardMode.loottarget != NULL) return &g_GuardMode.loottarget->pos;
    pad = guardmodeRoutePad();
    if (pad >= 10000) return &g_CurrentSetup.boundpads[pad - 10000].pos;
    return &g_CurrentSetup.pads[pad].pos;
}

s32 guardmodeCanFire(ChrRecord *chr)
{
    s32 slot;
    if (!guardmodeActive()) return TRUE;
    if (!guardmodeIsBond(chr))
    {
        slot = chr != NULL ? guardmodeSlot(chr->chrnum) : -1;
        if (slot < 0) return TRUE;
        return g_GuardMode.result == 0 && g_GuardMode.combatstarted
            && guardmodeTarget(chr) != NULL && g_GlobalTimer >= allynextshot[slot];
    }
    return g_GuardMode.result == 0 && g_GuardMode.combatstarted && !chrIsDead(chr)
        && (!guardmodeBondInTank(chr) || tankhandgunfiring) && g_GuardMode.tankphase != 2
        && guardmodeTarget(chr) != NULL && g_GlobalTimer >= g_GuardMode.reloadat
        && g_GlobalTimer >= g_GuardMode.nextshot;
}

static s32 guardmodeMagazineSize(s32 item)
{
    if (item == ITEM_WPPK || item == ITEM_WPPKSIL) return 7;
    if (item == ITEM_TT33) return 8;
    if (item == ITEM_SKORPION) return 20;
    if (item == ITEM_FNP90) return 80;
    if (item == ITEM_UZI) return 32;
    return 30;
}

void guardmodeShotFired(ChrRecord *chr)
{
    s32 i;
    s32 slot;
    ChrRecord *other;
    f32 range;
    if (!guardmodeActive()) return;
    if (!guardmodeIsBond(chr))
    {
        slot = guardmodeSlot(chr->chrnum);
        if (slot >= 0)
        {
            g_GuardMode.guardshots[slot]++;
            allynextshot[slot] = g_GlobalTimer + 8;
            if (++allyburst[slot] >= 4)
            {
                allyburst[slot] = 0;
                allynextshot[slot] = g_GlobalTimer + 60;
            }
        }
        return;
    }
    range = g_GuardMode.bondweapon == ITEM_WPPKSIL ? 1000.0f : 2400.0f;
    for (i = 0; i < 10; i++)
    {
        other = chrFindByLiteralId(guardids[i]);
        if (!taken[i] && guardmodeAlive(other)
            && guardmodeDistance2(chr->prop, other->prop) < range * range)
        {
            g_GuardMode.guardalerts[i] = g_GlobalTimer + 180;
            nextdecision[i] = 0;
        }
    }
    g_GuardMode.shots++;
    g_GuardMode.nextshot = g_GlobalTimer
        + (g_GuardMode.bondweapon <= ITEM_TT33 ? 20 : 10);
    if (--g_GuardMode.clip == 0)
    {
        g_GuardMode.clip = guardmodeMagazineSize(g_GuardMode.bondweapon);
        g_GuardMode.reloadat = g_GlobalTimer + 105;
        g_GuardMode.reloads++;
        chrStopFiring(chr);
    }
}

static s32 guardmodeCanSee(ChrRecord *chr, PropRecord *target)
{
    coord3d eye;
    coord3d point;
    if (target == NULL || target->stan == NULL || chr->prop->stan == NULL) return FALSE;
    eye = chr->prop->pos;
    eye.y = chr->ground + chr->chrheight * 0.88f;
    return guardmodeExposedPoint(chr, target, &eye, &point);
}

static void guardmodeArm(ChrRecord *chr, ITEM_IDS item)
{
    if (chrGetEquippedWeaponPropWithCheck(chr, GUNRIGHT) == NULL)
    {
        something_with_generating_object(chr, getPropForHeldItem(item), item, 0, NULL, NULL);
    }
    chr->chrflags |= CHRFLAG_CAN_SHOOT_CHRS;
    chr->chrflags &= ~CHRFLAG_CLONE;
    chr->grenadeprob = 0;
    chr->alertness = 255;
}

static s32 guardmodeWeaponRank(s32 item)
{
    if (item == ITEM_FNP90 || item == ITEM_M16) return 3;
    if (item >= ITEM_SKORPION && item <= ITEM_RUGER) return 2;
    return 1;
}

void guardmodeRecordKill(ChrRecord *chr)
{
    s32 slot;
    if (!guardmodeActive() || g_GuardModeShooter != g_GuardMode.bond
        || chr == g_GuardMode.bond || chr->damage < chr->maxdamage) return;
    slot = guardmodeSlot(chr->chrnum);
    if (slot >= 0 && !taken[slot] && lootweapons[slot] == NULL)
        lootweapons[slot] = chrGetEquippedWeaponPropWithCheck(chr, GUNRIGHT);
}

static void guardmodeDropPlayerWeapon(void)
{
    ObjectRecord *obj;
    s32 slot = guardmodeSlot(g_GuardMode.currentguard);
    s32 item = getCurrentPlayerWeaponId(GUNRIGHT);
    coord3d pos = g_CurrentPlayer->prop->pos;
    Mtxf matrix;
    if (slot < 0 || item < ITEM_WPPK || item > ITEM_RUGER) return;
    obj = create_new_item_instance_of_model(getPropForHeldItem(item), item);
    if (obj == NULL || obj->prop == NULL) return;
    pos.y = g_CurrentPlayer->stanHeight + 6.0f;
    matrix_4x4_set_identity(&matrix);
    matrix_scalar_multiply(obj->model->scale, matrix.m[0]);
    objChangeShading(obj, &pos, &matrix, g_CurrentPlayer->prop->stan);
    chrpropActivate(obj->prop);
    chrpropEnable(obj->prop);
    lootweapons[slot] = obj->prop;
    g_GuardMode.playerdrops++;
}

static void guardmodeLootTick(ChrRecord *chr)
{
    s32 i;
    s32 bestslot = -1;
    f32 best = 600.0f * 600.0f;
    f32 distance;
    PropRecord *prop;
    PropRecord *old;
    WeaponObjRecord *weapon;
    StandTile *line;
    PropRecord *previous = g_GuardMode.loottarget;
    coord3d origin = chr->prop->pos;
    coord3d point;
    g_GuardMode.loottarget = NULL;
    for (i = 0; i < 10; i++)
    {
        prop = lootweapons[i];
        if (prop == NULL || prop->type != PROP_TYPE_WEAPON || prop->obj == NULL
            || prop->obj->model == NULL || prop->parent != NULL || prop->stan == NULL
            || (prop->obj->runtime_bitflags & RUNTIMEBITFLAG_REMOVE)) continue;
        weapon = (WeaponObjRecord *)prop->obj;
        if (guardmodeWeaponRank(weapon->weaponnum) <= guardmodeWeaponRank(g_GuardMode.bondweapon)) continue;
        distance = guardmodeHorizontalDistance2(&chr->prop->pos, &prop->pos);
        line = chr->prop->stan;
        point = prop->pos;
        point.y += 80.0f;
        if (distance < best && chr->ground - prop->pos.y < 140.0f
            && prop->pos.y - chr->ground < 140.0f
            && walkTilesBetweenPoints_NoCallback(&line, chr->prop->pos.x,
                chr->prop->pos.z, prop->pos.x, prop->pos.z)
            && line == prop->stan && guardmodeClearPoint(chr, prop, &origin, &point))
        {
            best = distance;
            bestslot = i;
        }
    }
    if (bestslot < 0) return;
    prop = lootweapons[bestslot];
    if (previous != prop) lootdeadline = g_GlobalTimer + 180;
    if (g_GlobalTimer >= lootdeadline)
    {
        /* A blocked optional weapon must never hold up the objective route. */
        lootweapons[bestslot] = NULL;
        return;
    }
    if (best > 150.0f * 150.0f)
    {
        g_GuardMode.loottarget = prop;
        return;
    }
    /* Match the retail ChrEquipObject ownership operations. The real dropped
     * object is attached to Bond only after he physically reaches it. */
    old = chrGetEquippedWeaponPropWithCheck(chr, GUNRIGHT);
    chrStopFiring(chr);
    if (old != NULL)
    {
        objDetach(old);
        objFreePermanently(old->obj, TRUE);
    }
    objFreeEmbedmentOrProjectile(prop);
    chrpropDeregisterRooms(prop);
    chrpropDelist(prop);
    chrpropDisable(prop);
    weapon = (WeaponObjRecord *)prop->obj;
    weapon->flags &= ~(PROPFLAG_WEAPON_LEFTHANDED | PROPFLAG_CONCEAL_GUN | PROPFLAG_IS_DOUBLE);
    if (chrEquipWeapon(weapon, chr))
    {
        g_GuardMode.bondweapon = weapon->weaponnum;
        g_GuardMode.clip = guardmodeMagazineSize(weapon->weaponnum);
        g_GuardMode.reloadat = g_GlobalTimer + 30;
        g_GuardMode.nextshot = g_GuardMode.reloadat;
        g_GuardMode.weaponpickups++;
        check_set_actor_standing_still(chr, TARGET_COMPASS, 0);
    }
    lootweapons[bestslot] = NULL;
}

static s32 guardmodeEligible(s32 slot, ChrRecord *chr)
{
    /* Exclude the actual starting-barn guard, not a moving distance filter.
     * All remaining living guards stay eligible even after changing position. */
    return guardids[slot] != 24 && !taken[slot] && guardmodeAlive(chr)
        && chr->damage < chr->maxdamage;
}

static s32 guardmodeCountAvailable(void)
{
    s32 i;
    s32 count = 0;
    g_GuardMode.eligiblemask = 0;
    for (i = 0; i < 10; i++)
        if (guardmodeEligible(i, chrFindByLiteralId(guardids[i])))
        {
            count++;
            g_GuardMode.eligiblemask |= 1 << i;
        }
    return count;
}

static void guardmodeFinish(s32 result)
{
    if (g_GuardMode.result != 0) return;
    g_GuardMode.result = result;
    g_GuardMode.finishat = g_GlobalTimer + 180;
    /* Keep the stage clock advancing for the result-screen countdown. */
    if (result == 1)
    {
        objectiveregisters1 |= GUARDMODE_WIN_FLAG;
        g_isBondKIA = FALSE;
        hudmsgTopShow("BOND ELIMINATED - RUNWAY DEFENDED");
    }
    else
    {
        objectiveregisters1 |= GUARDMODE_FAIL_FLAG;
        g_isBondKIA = FALSE;
        hudmsgTopShow(result == 2 ? "BOND ESCAPED - MISSION FAILED" : "NO GUARDS AVAILABLE - MISSION FAILED");
    }
}

/* Use the native multiplayer new-life reset, then copy an actual living
 * guard's location and appearance. Removing that NPC prevents a duplicate
 * body at the controlled guard's position. Consumed guards cannot be reused. */
static s32 guardmodeTakeGuard(s32 first)
{
    s32 choices[10];
    s32 count = 0;
    s32 i;
    s32 slot;
    ChrRecord *chr;
    PropRecord *weapon;
    ITEM_IDS item = ITEM_AK47;
    coord3d pos;
    StandTile *stan;
    f32 yaw;
    f32 ground;
    f32 health;
    s32 distant = 0;
    coord3d oldpos = g_CurrentPlayer->prop->pos;

    for (i = 0; i < 10; i++)
    {
        chr = chrFindByLiteralId(guardids[i]);
        if ((!first || guardids[i] == 20 || guardids[i] == 21)
            && guardmodeEligible(i, chr))
        {
            choices[count++] = i;
            if (guardmodeHorizontalDistance2(&oldpos, &chr->prop->pos) > 1200.0f * 1200.0f) distant++;
        }
    }
    if (count == 0) return FALSE;
    /* Prefer a different part of the level after a death, while always
     * transferring into a living, existing NPC. Never use Bond's start pad. */
    if (!first && distant != 0)
    {
        count = 0;
        for (i = 0; i < 10; i++)
        {
            chr = chrFindByLiteralId(guardids[i]);
            if (guardmodeEligible(i, chr)
                && guardmodeHorizontalDistance2(&oldpos, &chr->prop->pos) > 1200.0f * 1200.0f)
                choices[count++] = i;
        }
    }
    slot = choices[randomGetNext() % count];
    chr = chrFindByLiteralId(guardids[slot]);
    pos = chr->prop->pos;
    g_GuardMode.spawnpos = pos;
    stan = chr->prop->stan;
    /* Player theta runs clockwise; character model yaw runs anticlockwise. */
    yaw = M_TAU_F - getsubroty(chr->model);
    health = (chr->maxdamage - chr->damage) / chr->maxdamage;
    g_GuardMode.body = chr->bodynum;
    g_GuardMode.head = chr->headnum;
    g_GuardMode.currentguard = chr->chrnum;
    weapon = chrGetEquippedWeaponPropWithCheck(chr, GUNRIGHT);
    if (weapon != NULL) item = ((WeaponObjRecord *)weapon->obj)->weaponnum;
    if (item < ITEM_WPPK || item > ITEM_RUGER) item = ITEM_AK47;
    taken[slot] = TRUE;
    chrStopFiring(chr);
    chr->chrflags |= CHRFLAG_HIDDEN | CHRFLAG_NO_AUTOAIM;
    chr->hidden |= CHRHIDDEN_REMOVE;

    bondviewRemovePlayerBody();
    mp_respawn_handler();
    /* Cinematic cleanup normally force-disarms for a few frames. The native
     * new-life reset has finished, so clear that old-life transient here. */
    g_bondviewForceDisarm = 0;
    g_isBondKIA = FALSE;
    bondinvReinitInv();
    for (i = 0; i < 30; i++) g_CurrentPlayer->ammoheldarr[i] = 0;
    bondinvAddInvItem(ITEM_FIST);
    bondinvAddInvItem(item);
    add_ammo_to_weapon(item, 90);
    starting_weapon[GUNLEFT] = ITEM_UNARMED;
    starting_weapon[GUNRIGHT] = item;
    currentPlayerEquipWeaponWrapper(GUNLEFT, ITEM_UNARMED);
    currentPlayerEquipWeaponWrapper(GUNRIGHT, item);

    ground = bondviewYPositionRelated(stan, pos.x, pos.z);
    pos.y = ground + g_CurrentPlayer->eyeheight;
    g_CurrentPlayer->stanHeight = ground;
    g_CurrentPlayer->field_70 = ground;
    g_CurrentPlayer->field_6C = ground / 0.17000002f;
    g_CurrentPlayer->vv_theta = yaw * 360.0f / M_TAU_F;
    g_CurrentPlayer->vv_costheta = cosf(yaw);
    g_CurrentPlayer->vv_sintheta = sinf(yaw);
    change_player_pos_to_target(&g_CurrentPlayer->field_488, &pos, stan);
    g_CurrentPlayer->field_488.theta_transform.x = -sinf(yaw);
    g_CurrentPlayer->field_488.theta_transform.y = 0.0f;
    g_CurrentPlayer->field_488.theta_transform.z = cosf(yaw);
    g_CurrentPlayer->prop->pos = pos;
    g_CurrentPlayer->bondprevpos = pos;
    g_CurrentPlayer->prop->stan = stan;
    g_CurrentPlayer->field_3B8.x = pos.x / 0.100000024f;
    g_CurrentPlayer->field_3B8.y = pos.y / 0.100000024f;
    g_CurrentPlayer->field_3B8.z = pos.z / 0.100000024f;
    g_CurrentPlayer->bondhealth = (g_GuardMode.difficulty >= DIFFICULTY_00 ? 0.625f : 0.375f) * health;
    if (g_CurrentPlayer->bondhealth < 0.05f) g_CurrentPlayer->bondhealth = 0.05f;
    g_CurrentPlayer->bondarmour = 0.0f;
    g_CurrentPlayer->oldhealth = g_CurrentPlayer->apparenthealth = g_CurrentPlayer->bondhealth;
    g_CurrentPlayer->oldarmour = g_CurrentPlayer->apparentarmour = 0.0f;
    g_CurrentPlayer->deathanimfinished = FALSE;
    g_CurrentPlayer->redbloodfinished = FALSE;
    g_CurrentPlayer->bonddead = FALSE;
    bondviewUpdatePlayerCollisionBounds();
    g_GuardMode.deathat = -1;
    g_GuardMode.deathcamera = 0;
    g_GuardMode.deathbloodframes = 0;
    g_GuardMode.tankcrushdeath = FALSE;
    g_GuardMode.remaining = guardmodeCountAvailable();
    g_GuardMode.introcount++;
    /* Preserve Bond's fight with another NPC across a player transfer. */
    /* A transfer is still the same mission. Keep the stage track alive. */
    if (get_mission_state() != MISSION_STATE_1) set_missionstate(MISSION_STATE_1);
    /* This is the original solo model animation + camera zoom, not a custom
     * fade. solo_char_load() reads the selected guard's body/head override. */
    bondviewSetCameraMode(CAMERAMODE_SWIRL);
    return TRUE;
}

void guardmodePlayerAppearance(s32 *body, s32 *head)
{
    if (guardmodeActive())
    {
        *body = g_GuardMode.body;
        *head = g_GuardMode.head;
    }
}

void guardmodeStageReady(void)
{
    s32 i;
    ChrRecord *chr;
    if (!guardmodeActive()) return;
    g_GuardMode.bond = chrFindByLiteralId(GUARDMODE_BOND_ID);
    if (!guardmodeAlive(g_GuardMode.bond)) return;
    guardmodeArm(g_GuardMode.bond, ITEM_WPPKSIL);
    g_GuardMode.bondweapon = ITEM_WPPKSIL;
    g_GuardMode.clip = guardmodeMagazineSize(ITEM_WPPKSIL);
    g_GuardMode.bond->maxdamage = 160.0f;
    g_GuardMode.bond->damage = 0.0f;
    g_GuardMode.bond->accuracyrating = 65;
    g_GuardMode.bond->speedrating = 100;
    g_GuardMode.bond->arghrating = 0;
    g_GuardMode.bond->chrflags |= CHRFLAG_INCREASE_RUNNING_SPEED;
    g_GuardMode.difficulty = selected_difficulty;
    g_GuardMode.requiredmask = selected_difficulty >= DIFFICULTY_00 ? 15
        : (selected_difficulty >= DIFFICULTY_SECRET ? 1 : 0);
    g_GuardMode.tank = (TankRecord *)setupGetPtrToCommandByIndex(119);
    if (g_GuardMode.tank != NULL && g_GuardMode.tank->type != PROPDEF_TANK)
        g_GuardMode.tank = NULL;
    if (g_GuardMode.tank != NULL && g_GuardMode.tank->prop != NULL)
        g_GuardMode.tankstart = g_GuardMode.tank->prop->pos;
    g_GuardMode.objectivetarget = -1;
    g_GuardMode.keydoor = (DoorRecord *)setupGetPtrToCommandByIndex(124);
    if (g_GuardMode.keydoor != NULL && g_GuardMode.keydoor->type != PROPDEF_DOOR)
        g_GuardMode.keydoor = NULL;
    for (i = 0; i < 10; i++)
    {
        chr = chrFindByLiteralId(guardids[i]);
        if (guardmodeAlive(chr))
        {
            guardmodeArm(chr, ITEM_AK47);
            chr->maxdamage = 3.0f;
            chr->damage = 0.0f;
            /* Leave ACT_INIT intact so retail initializes the floor/pose. */
            chr->sleep = 0;
        }
    }
    g_GuardMode.initialized = TRUE;
    if (!guardmodeTakeGuard(TRUE)) guardmodeFinish(3);
}

void guardmodePlayerDied(void)
{
    if (guardmodeActive() && g_GuardMode.result == 0 && g_GuardMode.deathat < 0)
    {
        g_GuardMode.deaths++;
        g_GuardMode.deathat = g_GlobalTimer;
        g_GuardMode.deathcamera = 0;
        g_GuardMode.deathbloodframes = 0;
        g_GuardMode.tankcrushdeath = FALSE;
        g_CurrentPlayer->redbloodfinished = TRUE;
        if (g_GuardModeShooter == g_GuardMode.bond) guardmodeDropPlayerWeapon();
        if (g_GuardMode.targetid == -1) g_GuardMode.targetid = -2;
        if (g_GuardMode.retaliateid == -1)
        {
            g_GuardMode.retaliateid = -2;
            g_GuardMode.retaliateuntil = 0;
        }
        nextdecision[10] = 0;
        g_isBondKIA = FALSE;
    }
}

void guardmodeDeathCameraFinished(void)
{
    if (guardmodeActive() && g_GuardMode.deathat >= 0 && g_GuardMode.deathcamera == 1)
    {
        g_GuardMode.deathcamera = 2;
        g_GuardMode.deathcamerafinishes++;
        g_CurrentPlayer->deathanimfinished = TRUE;
    }
}

void guardmodeFrame(void)
{
    s32 i;
    ChrRecord *chr;
    if (!guardmodeActive() || !g_GuardMode.initialized || g_GuardMode.lastframe == g_GlobalTimer) return;
    g_GuardMode.lastframe = g_GlobalTimer;
    if (g_CameraMode == CAMERAMODE_FP) g_GuardMode.combatstarted = TRUE;
    /* Standing retail guards sleep between script decisions. Wake these
     * defenders each frame so first sight and target loss are handled quickly. */
    for (i = 0; i < 10; i++)
    {
        chr = chrFindByLiteralId(guardids[i]);
        if (!taken[i] && guardmodeAlive(chr)) chr->sleep = 0;
    }
    if (g_GuardMode.result != 0)
    {
        if (g_GlobalTimer >= g_GuardMode.finishat) bossReturnTitleStage();
        return;
    }
    g_GuardMode.bondhealth = g_GuardMode.bond->maxdamage - g_GuardMode.bond->damage;
    g_GuardMode.remaining = guardmodeCountAvailable();
    if (chrIsDead(g_GuardMode.bond))
    {
        guardmodeFinish(1);
        return;
    }
    if (g_GuardMode.deathat >= 0)
    {
        if (g_GuardMode.deathcamera == 0)
        {
            /* Defer until the native kill routine has saved death position
             * and view angles. Its one body animation owns transfer timing. */
            g_GuardMode.deathcamera = 1;
            g_GuardMode.deathcamerastart = g_GlobalTimer;
            g_GuardMode.deathcamerastarts++;
            bondviewGuardDeathCameraStart();
        }
        else if (g_GuardMode.deathcamera == 2)
        {
            if (!guardmodeTakeGuard(FALSE)) guardmodeFinish(3);
        }
        else if (g_GlobalTimer - g_GuardMode.deathcamerastart > 600)
        {
            /* A missing/corrupt presentation model cannot strand a life. */
            guardmodeDeathCameraFinished();
        }
    }
}

static void guardmodeObjectiveTick(ChrRecord *chr)
{
    ObjectRecord *key = objFindByTagId(2);
    ObjectRecord *plane = objFindByTagId(3);
    StandTile *keystan = g_CurrentSetup.pads[80].stan;
    s32 inkeyroom = chr->prop->stan != NULL && keystan != NULL
        && chr->prop->stan->room == keystan->room;
    StandTile *line = chr->prop->stan;
    guardmodeObjectiveMaskTick();
    if (inkeyroom) g_GuardMode.keyroomentered = TRUE;
    if (!g_GuardMode.keytaken && key != NULL && key->prop != NULL
        && g_GuardMode.route == 4 && inkeyroom
        && guardmodeHorizontalDistance2(&chr->prop->pos, &key->prop->pos) < 200.0f * 200.0f
        && walkTilesBetweenPoints_NoCallback(&line, chr->prop->pos.x,
            chr->prop->pos.z, key->prop->pos.x, key->prop->pos.z))
    {
        /* The key belongs to the AI Bond, never to the controlled guard. */
        g_GuardMode.keytaken = TRUE;
        g_GuardMode.keypickuppos = chr->prop->pos;
        g_GuardMode.keypickuproom = chr->prop->stan->room;
        objFreePermanently(key, TRUE);
        g_GuardMode.route = 5;
        routecommand = -1;
        hudmsgTopShow("BOND TOOK THE AIRCRAFT KEY");
    }
    if ((g_GuardMode.tankphase == 0 || g_GuardMode.tankphase >= 5)
        && g_GuardMode.route != 4 && g_GuardMode.route < 12
        && (g_GuardMode.route < 4 || g_GuardMode.keytaken)
        && chr->prop->stan != NULL
        && g_CurrentSetup.pads[routesteps[g_GuardMode.route]].stan != NULL
        && (g_GuardMode.route >= 9 || chr->prop->stan->room
            == g_CurrentSetup.pads[routesteps[g_GuardMode.route]].stan->room)
        && guardmodeHorizontalDistance2(&chr->prop->pos,
            &g_CurrentSetup.pads[routesteps[g_GuardMode.route]].pos)
                < (g_GuardMode.route < 9 ? 110.0f * 110.0f : 400.0f * 400.0f))
    {
        g_GuardMode.route++;
        routecommand = -1;
    }
    if (g_GuardMode.requiredmask && g_GuardMode.keytaken && g_GuardMode.route >= 9
        && g_GuardMode.tankphase == 0)
    {
        g_GuardMode.tankphase = 1;
        routecommand = -1;
        g_GuardMode.loottarget = NULL;
    }
    if (g_GuardMode.keytaken && g_GuardMode.route == 12 && plane != NULL && plane->prop != NULL
        && (g_GuardMode.destroyedmask & g_GuardMode.requiredmask) == g_GuardMode.requiredmask
        && (g_GuardMode.requiredmask == 0 || g_GuardMode.tankphase == 5)
        && guardmodeHorizontalDistance2(&chr->prop->pos, &plane->prop->pos) < 500.0f * 500.0f)
        guardmodeFinish(2);
}

static PropRecord *guardmodeChooseTarget(ChrRecord *chr)
{
    s32 ids[11];
    s32 count = 0;
    s32 i;
    ChrRecord *other;
    PropRecord *target = guardmodeTarget(chr);
    if (g_GlobalTimer < g_GuardMode.retaliateuntil && g_GuardMode.retaliateid != -2)
    {
        g_GuardMode.targetid = g_GuardMode.retaliateid;
        target = guardmodeTarget(chr);
        if (target != NULL) return target; /* Hit stimulus bypasses acquisition range. */
        g_GuardMode.retaliateid = -2;
    }
    if (target != NULL && g_GlobalTimer < g_GuardMode.targetuntil
        && guardmodeCanSee(chr, target)) return target;
    if (guardmodePlayerTargetable() && guardmodeCanSee(chr, g_CurrentPlayer->prop)
        && guardmodeDistance2(chr->prop, g_CurrentPlayer->prop) < 2400.0f * 2400.0f)
        ids[count++] = -1;
    for (i = 0; i < 10; i++)
    {
        other = chrFindByLiteralId(guardids[i]);
        if (!taken[i] && guardmodeAlive(other) && guardmodeCanSee(chr, other->prop)
            && guardmodeDistance2(chr->prop, other->prop) < 2400.0f * 2400.0f)
            ids[count++] = guardids[i];
    }
    if (count == 0)
    {
        g_GuardMode.targetid = -2;
        return NULL;
    }
    /* Each visible guard has an equal chance, including the player. Hold
     * the choice for two seconds rather than oscillating between neighbours. */
    g_GuardMode.targetid = ids[randomGetNext() % count];
    g_GuardMode.targetuntil = g_GlobalTimer + 120;
    chr->shotbondsum = 0.0f;
    return guardmodeTarget(chr);
}

static s32 guardmodeNeedsNavigation(ChrRecord *chr)
{
    StandTile *stan = chr->prop->stan;
    coord3d *goal = guardmodeMoveTarget(chr);
    if (goal == NULL) return FALSE;
    if (chr->invalidmove == 1 || chr->lastmoveok60 < g_GlobalTimer - 60)
        navigationuntil = g_GlobalTimer + 120;
    return g_GlobalTimer < navigationuntil
        || !walkTilesBetweenPoints_NoCallback(&stan, chr->prop->pos.x, chr->prop->pos.z, goal->x, goal->z);
}

static s32 guardmodeNearKeyDoor(ChrRecord *chr)
{
    DoorRecord *door = g_GuardMode.keydoor;
    return g_GuardMode.route < 9 && door != NULL && door->prop != NULL
        && guardmodeHorizontalDistance2(&chr->prop->pos, &door->prop->pos) < 650.0f * 650.0f;
}

static void guardmodeDoorTick(ChrRecord *chr)
{
    DoorRecord *door = g_GuardMode.keydoor;
    if (!guardmodeNearKeyDoor(chr)) return;
    /* Attack-walk and sidestep do not execute the retail GoPos door handler.
     * Open explicitly, without restarting an opening door each AI decision. */
    door->openedTime = g_GlobalTimer;
    if (door->openstate == DOORSTATE_CLOSING || door->openstate == DOORSTATE_WAITING
        || (door->openstate == DOORSTATE_STATIONARY && door->openPosition < door->maxFrac * 0.98f))
    {
        doorsChooseSwingDirection(chr->prop, door);
        doorActivate(door, DOORSTATE_OPENING);
        g_GuardMode.dooropens++;
    }
}

static void guardmodeRecoverRoute(ChrRecord *chr)
{
    if (g_GuardMode.loottarget != NULL) return;
    /* The retail offscreen shortcut advances a distance counter before it
     * changes position. Let its native conversion back to walking complete. */
    if (chr->actiontype == ACT_GOPOS && chr->act_gopos.waydata.mode == WAYMODE_MAGIC)
    {
        progressat = g_GlobalTimer;
        return;
    }
    if (progressat == 0
        || guardmodeHorizontalDistance2(&chr->prop->pos, &progresspos) > 120.0f * 120.0f)
    {
        progresspos = chr->prop->pos;
        progressat = g_GlobalTimer;
    }
    else if (g_GlobalTimer - progressat > 180)
    {
        /* Rebuild native waypoints and movement animation after a blocked or
         * stale route. Never move Bond by teleporting him to the objective. */
        if (chrGoToPad(chr, guardmodeRoutePad(), SPEED_RUN))
            routecommand = g_GuardMode.route;
        navigationuntil = g_GlobalTimer + 120;
        progressat = g_GlobalTimer;
    }
}

static s32 guardmodeTrySidestep(ChrRecord *chr, PropRecord *target)
{
    f32 oldyaw;
    f32 yaw;
    s32 side;
    if (chr->actiontype == ACT_SIDESTEP) return TRUE;
    if (g_GlobalTimer < nextsidestep) return FALSE;
    oldyaw = getsubroty(chr->model);
    yaw = atan2f(target->pos.x - chr->prop->pos.x,
        target->pos.z - chr->prop->pos.z);
    setsubroty(chr->model, yaw);
    side = stepside;
    if (!sub_GAME_7F02A1E8(chr, side, 120.0f)) side = 1 - side;
    if (sub_GAME_7F02A1E8(chr, side, 120.0f))
    {
        /* Native sideways root motion and character-width collision checks.
         * Continue aiming/fire during the animation; a reload only gates fire. */
        chrlvSideStepAnimationRelated(chr, side);
        stepside = 1 - side;
        nextsidestep = g_GlobalTimer + 240;
        g_GuardMode.strafes++;
        return TRUE;
    }
    setsubroty(chr->model, oldyaw);
    nextsidestep = g_GlobalTimer + 30;
    return FALSE;
}

/* Native movement/aiming/tracing, with one latched target for the AI Bond. */
s32 guardmodeAi(ChrRecord *chr)
{
    s32 slot;
    s32 type;
    s32 id;
    ChrRecord *targetchr = NULL;
    PropRecord *target = NULL;
    f32 best = 2500.0f * 2500.0f;
    if (!guardmodeActive()) return FALSE;
    if (chr->prop == NULL) return TRUE; /* Suspend retail level/clone scripts. */
    if (chr->prop->type != PROP_TYPE_CHR) return FALSE;
    slot = guardmodeSlot(chr->chrnum);
    if (chr->chrnum == GUARDMODE_BOND_ID) slot = 10;
    if (slot < 0) return TRUE;
    if (!g_GuardMode.initialized || !guardmodeAlive(chr)) return TRUE;
    if (g_GuardMode.result != 0 || !g_GuardMode.combatstarted)
    {
        chrStopFiring(chr);
        check_set_actor_standing_still(chr, 0, 0);
        return TRUE;
    }
    if (chr->actiontype == ACT_ARGH || chr->actiontype == ACT_PREARGH) return TRUE;
    if (g_GlobalTimer < nextdecision[slot]) return TRUE;
    nextdecision[slot] = g_GlobalTimer + 6;
    g_GuardMode.aisteps++;
    if (slot == 10)
    {
        guardmodeDoorTick(chr);
        guardmodeObjectiveTick(chr);
        if (g_GuardMode.result != 0) return TRUE;
        if (guardmodeVehicleControl(chr)) return TRUE;
        if (g_GuardMode.tankphase != 1) guardmodeLootTick(chr);
        guardmodeRecoverRoute(chr);
        target = guardmodeChooseTarget(chr);
    }
    else
    {
        targetchr = g_GuardMode.bond;
        if (guardmodeAlive(targetchr)
            && (guardmodeCanSee(chr, targetchr->prop)
                || g_GlobalTimer < g_GuardMode.guardalerts[slot]))
        {
            best = guardmodeDistance2(chr->prop, targetchr->prop);
            if (best < 7000.0f * 7000.0f) target = targetchr->prop;
        }
        allyengaged[slot] = target != NULL;
    }
    if (target != NULL)
    {
        type = slot == 10 && g_GuardMode.targetid == -1 ? TARGET_BOND : TARGET_CHR;
        id = type == TARGET_BOND ? 0 : target->chr->chrnum;
        chr->seen_bond_time = g_GlobalTimer;
        if (slot == 10 && chr->actiontype == ACT_SIDESTEP) return TRUE;
        if (slot == 10 && g_GuardMode.tankphase != 1 && g_GuardMode.loottarget == NULL
            && !guardmodeNearKeyDoor(chr) && !guardmodeNeedsNavigation(chr)
            && guardmodeTrySidestep(chr, target)) return TRUE;
        if (slot == 10 && guardmodeNeedsNavigation(chr))
        {
            if (chr->actiontype != ACT_GOPOS || routecommand != g_GuardMode.route)
                if (chrGoToPad(chr, guardmodeRoutePad(), SPEED_RUN))
                    routecommand = g_GuardMode.route;
        }
        else if (slot == 10 && guardmodeMoveTarget(chr) != NULL
            && guardmodeHorizontalDistance2(&chr->prop->pos, guardmodeMoveTarget(chr))
                > (g_GuardMode.route < 9 ? 60.0f * 60.0f : 350.0f * 350.0f)
            && chr->invalidmove != 1)
        {
            if (chr->actiontype != ACT_ATTACKWALK) chrlvInitActAttackWalk(chr, SPEED_RUN);
            /* Continuous run-and-fire; reload pauses are owned by the clip. */
            chr->act_attackwalk.clock_timer34 = chr->act_attackwalk.clock_timer30 + 600;
        }
        else if (chr->actiontype != ACT_ATTACK)
        {
            if (slot != 10)
                setsubroty(chr->model, atan2f(target->pos.x - chr->prop->pos.x,
                    target->pos.z - chr->prop->pos.z));
            if (actor_aim_at_actor(chr, type, id) && slot != 10)
            {
                /* Alert allies bring their existing rifle animation up
                 * promptly, before Bond can finish a whole pistol magazine. */
                f32 ready = chr->act_attack.animfloats->shoot_start_frame - 4.0f;
                if (ready > modelGetAnimFrame(chr->model)) modelSetAnimFrame(chr->model, ready);
            }
        }
        else
        {
            if (!actor_fire_or_aim_at_target_update(chr, type, id))
            {
                chr->act_attack.attacktype = type;
                chr->act_attack.entityid = id;
            }
        }
    }
    else if (slot == 10)
    {
        /* Keep precise, physical movement through the interior even when the
         * player dies or the camera moves away. Retail offscreen GoPos uses
         * the exterior waypoint graph and can turn back before the desk. */
        if (!guardmodeNeedsNavigation(chr) && guardmodeMoveTarget(chr) != NULL)
        {
            if (chr->actiontype != ACT_ATTACKWALK) chrlvInitActAttackWalk(chr, SPEED_RUN);
            chr->act_attackwalk.clock_timer34 = chr->act_attackwalk.clock_timer30 + 600;
        }
        else if (chr->actiontype != ACT_GOPOS || routecommand != g_GuardMode.route)
        {
            if (chrGoToPad(chr, guardmodeRoutePad(), SPEED_RUN))
                routecommand = g_GuardMode.route;
        }
    }
    else
    {
        chrStopFiring(chr);
        check_set_actor_standing_still(chr, 0, 0);
    }
    return TRUE;
}

s32 guardmodeAllowDamage(ChrRecord *victim, s32 playerhit)
{
    if (!guardmodeActive()) return TRUE;
    if (g_GuardMode.result != 0 || !g_GuardMode.combatstarted) return FALSE;
    if (playerhit) return guardmodePlayerTargetable() && victim == g_GuardMode.bond;
    if (g_GuardModeShooter != NULL)
    {
        if (g_GuardModeShooter == g_GuardMode.bond)
            return victim != g_GuardMode.bond;
        return victim == g_GuardMode.bond;
    }
    return TRUE;
}

s32 guardmodeAllowAutoAim(ChrRecord *chr)
{
    return !guardmodeActive() || chr == g_GuardMode.bond;
}

static s32 guardmodeRoutePad(void)
{
    if (g_GuardMode.tankphase == 1) return 44;
    return routesteps[g_GuardMode.route];
}

s32 guardmodeBondInTank(ChrRecord *chr)
{
    return guardmodeIsBond(chr)
        && (g_GuardMode.tankphase == 3 || g_GuardMode.tankphase == 4);
}

static void guardmodeObjectiveMaskTick(void)
{
    ObjectRecord *obj;
    s32 i;
    s32 mask = 0;
    for (i = 0; i < 4; i++)
    {
        obj = objFindByTagId(objectivetags[i]);
        if (obj != NULL && (obj->prop == NULL || !objIsHealthy(obj))) mask |= 1 << i;
    }
    if ((mask & g_GuardMode.requiredmask) != (g_GuardMode.destroyedmask & g_GuardMode.requiredmask))
        hudmsgTopShow((mask & 1) && !(g_GuardMode.destroyedmask & 1)
            ? "BOND DESTROYED THE MISSILE BATTERY" : "BOND DESTROYED A HEAVY GUN");
    g_GuardMode.destroyedmask = mask;
}

static f32 guardmodeAngleDelta(f32 goal, f32 current)
{
    f32 delta = goal - current;
    while (delta > M_PI_F) delta -= M_TAU_F;
    while (delta < -M_PI_F) delta += M_TAU_F;
    return delta;
}

static f32 guardmodeTurnToward(f32 current, f32 goal, f32 speed)
{
    f32 delta = guardmodeAngleDelta(goal, current);
    if (delta > speed) delta = speed;
    if (delta < -speed) delta = -speed;
    return current + delta;
}

static s32 guardmodeVehicleControl(ChrRecord *chr)
{
    TankRecord *tank = g_GuardMode.tank;
    f32 distance;
    f32 reach;
    if (g_GuardMode.tankphase == 1 && tank != NULL && tank->prop != NULL)
    {
        distance = guardmodeHorizontalDistance2(&chr->prop->pos, &tank->prop->pos);
        reach = objGetWidth((ObjectRecord *)tank) + 180.0f;
        if (distance < reach * reach)
        {
            g_GuardMode.tankphase = 2;
            tankat = g_GlobalTimer;
            tankyaw = atan2f(-tank->mtx.m[2][0], tank->mtx.m[2][2]);
            tankheight = tank->prop->pos.y - stanGetPositionYValue(tank->prop->stan,
                tank->prop->pos.x, tank->prop->pos.z);
            chrStopFiring(chr);
            check_set_actor_standing_still(chr, TARGET_COMPASS, 0);
        }
    }
    return g_GuardMode.tankphase >= 2 && g_GuardMode.tankphase <= 4;
}

static s32 guardmodeTankCanMove(coord3d *pos, StandTile *stan, f32 yaw)
{
    PropRecord *saved = g_PlayerTankProp;
    coord3d savedoffset = g_TankModelPositionOffset;
    f32 savedturret = g_TankTurretOrientationAngleRad;
    s32 allowed;
    /* Reuse the retail tank's full footprint and cannon collision check.
     * The observer never becomes the tank driver; restore every temporary
     * collision-context global before returning to the ordinary player. */
    g_PlayerTankProp = g_GuardMode.tank->prop;
    g_TankModelPositionOffset.x = 0.0f;
    g_TankModelPositionOffset.y = 0.0f;
    g_TankModelPositionOffset.z = 0.0f;
    g_TankTurretOrientationAngleRad = g_GuardMode.tank->turret_orientation_angle;
    allowed = bondviewCallTankCollisionStatus(pos, stan, yaw);
    g_TankTurretOrientationAngleRad = savedturret;
    g_TankModelPositionOffset = savedoffset;
    g_PlayerTankProp = saved;
    return allowed;
}

static ModelNode *guardmodeFindBoxNode(ModelNode *node)
{
    ModelNode *found;
    for (; node != NULL; node = node->Next)
    {
        if ((node->Opcode & 255) == MODELNODE_OPCODE_BBOX) return node;
        if (node->Child != NULL)
        {
            found = guardmodeFindBoxNode(node->Child);
            if (found != NULL) return found;
        }
    }
    return NULL;
}

static ModelNode *guardmodeObjectBoxNode(ObjectRecord *obj)
{
    ModelNode *node;
    if (obj->type == PROPDEF_AUTOGUN && obj->model->obj->Switches[2] != NULL)
    {
        /* Use the shootable rotating gun, rather than the empty centre of
         * the much larger root culling box on the two far emplacements. */
        node = guardmodeFindBoxNode(obj->model->obj->Switches[2]->Child);
        if (node != NULL) return node;
    }
    return guardmodeFindBoxNode(obj->model->obj->RootNode->Child);
}

static void guardmodeObjectBoundsMatrix(ObjectRecord *obj, Mtxf *world)
{
    ModelNode *bboxnode = guardmodeObjectBoxNode(obj);
    Mtxf turret;
    coord3d pivot;
    s32 index;
    matrix_4x4_copy(&obj->mtx, world);
    matrix_4x4_set_position(&obj->runtime_pos, world);
    if (obj->type != PROPDEF_AUTOGUN || obj->model->obj->RootNode->Child == NULL) return;
    if (bboxnode == NULL) return;
    index = modelFindNodeMtxIndex(bboxnode, 0);
    if (index != 1 && index != 2) return;
    pivot = *(coord3d *)obj->model->obj->Switches[1]->Data;
    mtx4TransformVecInPlace(world, &pivot);
    matrix_4x4_set_rotation_around_y(((AutogunRecord *)obj)->unk90 + M_PI_2F, &turret);
    matrix_scalar_multiply(obj->model->scale, turret.m[0]);
    matrix_4x4_set_position(&pivot, &turret);
    if (index == 1) matrix_4x4_copy(&turret, world);
    else
    {
        matrix_4x4_set_rotation_around_z(-((AutogunRecord *)obj)->unk9C, world);
        matrix_4x4_set_position((coord3d *)obj->model->obj->Switches[2]->Data, world);
        matrix_4x4_multiply_homogeneous_in_place(&turret, world);
    }
}

static void guardmodeObjectPoint(ObjectRecord *obj, coord3d *point)
{
    rect4f *polygon;
    s32 edges;
    s32 i;
    f32 top;
    f32 bottom;
    Mtxf world;
    ModelNode *node = guardmodeObjectBoxNode(obj);
    ModelRoData_BoundingBoxRecord *box = node == NULL ? NULL : node->Data;
    chraiGetCollisionBounds(obj->prop, &polygon, &edges, &top, &bottom);
    if (polygon != NULL && edges >= 3)
    {
        point->x = 0.0f;
        point->z = 0.0f;
        point->y = (top + bottom) * 0.5f;
        for (i = 0; i < edges; i++)
        {
            point->x += polygon->points[i].x / edges;
            point->z += polygon->points[i].y / edges;
        }
        return;
    }
    if (box != NULL)
    {
        point->x = (box->Bounds.xmin + box->Bounds.xmax) * 0.5f;
        point->y = (box->Bounds.ymin + box->Bounds.ymax) * 0.5f;
        point->z = (box->Bounds.zmin + box->Bounds.zmax) * 0.5f;
        guardmodeObjectBoundsMatrix(obj, &world);
        mtx4TransformVecInPlace(&world, point);
    }
    else *point = obj->runtime_pos;
}

static void guardmodeTankMuzzle(coord3d *point)
{
    TankRecord *tank = g_GuardMode.tank;
    coord3d *base = tank->model->obj->Switches[1]->Data;
    coord3d *pivot = tank->model->obj->Switches[3]->Data;
    coord3d *barrel = tank->model->obj->Switches[4]->Data;
    Mtxf rotation;
    *point = *barrel;
    matrix_4x4_set_rotation_around_x(-tank->turret_vertical_angle, &rotation);
    mtx4RotateVecInPlace(&rotation, point);
    point->x += pivot->x;
    point->y += pivot->y;
    point->z += pivot->z;
    matrix_4x4_set_rotation_around_y(-tank->turret_orientation_angle, &rotation);
    mtx4RotateVecInPlace(&rotation, point);
    point->x += base->x;
    point->y += base->y;
    point->z += base->z;
    mtx4RotateVecInPlace(&tank->mtx, point);
    point->x += tank->runtime_pos.x;
    point->y += tank->runtime_pos.y;
    point->z += tank->runtime_pos.z;
}

/* Retail uses render matrices for shootable gun meshes. Those matrices are
 * absent when the human guard is in another room. Trace AI tank rounds against
 * the actual target's oriented model bounds; native projectile
 * impact, damage and explosions still own the result. */
s32 guardmodeTankProjectileIgnores(PropRecord *projectile, PropRecord *candidate)
{
    ObjectRecord *round;
    if (!guardmodeActive() || g_GuardMode.tank == NULL || projectile == NULL
        || projectile->type != PROP_TYPE_WEAPON || projectile->obj == NULL) return FALSE;
    round = projectile->obj;
    if (!(round->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        || round->projectile->ownerprop != g_GuardMode.tank->prop) return FALSE;
    /* The retail player-tank exception uses g_PlayerTankProp. AI shells need
     * the same owner exclusion, including detailed onscreen model tests. */
    return candidate == g_GuardMode.tank->prop
        || (guardmodeBondInTank(g_GuardMode.bond) && candidate == g_GuardMode.bond->prop);
}

/* The same finite world-space segment used by native projectile motion.
 * Remote characters have no camera render matrices, so use their actual
 * collision width, floor and height rather than an absent rendered mesh. */
PropRecord *guardmodeTankCharacterCollision(PropRecord *projectile, coord3d *from,
    coord3d *to, coord3d *hit, coord3d *normal, f32 *bestdistance)
{
    PropRecord *closest = NULL;
    PropRecord *prop;
    ChrRecord *chr;
    coord3d delta;
    f32 minimum[3];
    f32 maximum[3];
    f32 width;
    f32 length;
    f32 entry;
    f32 leave;
    f32 near;
    f32 far;
    f32 swap;
    f32 distance;
    f32 side;
    s32 axis;
    s32 i;
    s32 j;
    if (!guardmodeActive() || g_GuardMode.tank == NULL
        || !guardmodeTankProjectileIgnores(projectile, g_GuardMode.tank->prop)) return NULL;
    delta.x = to->x - from->x;
    delta.y = to->y - from->y;
    delta.z = to->z - from->z;
    length = sqrtf(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z);
    if (length < 0.00001f) return NULL;
    for (i = 0; i < 11; i++)
    {
        if (i == 10)
        {
            if (!guardmodePlayerTargetable()) continue;
            prop = g_CurrentPlayer->prop;
            width = g_CurrentPlayer->field_488.collision_radius;
            minimum[1] = g_CurrentPlayer->stanHeight + 20.0f;
            maximum[1] = g_CurrentPlayer->stanHeight
                + bondviewGetPlayerDuckingHeightRelated(g_CurrentPlayer) + 10.0f;
        }
        else
        {
            chr = chrFindByLiteralId(guardids[i]);
            if (taken[i] || !guardmodeAlive(chr)) continue;
            prop = chr->prop;
            width = chr->chrwidth;
            minimum[1] = chr->ground + 20.0f;
            maximum[1] = chr->ground + chr->chrheight - 20.0f;
        }
        minimum[0] = prop->pos.x - width;
        maximum[0] = prop->pos.x + width;
        minimum[2] = prop->pos.z - width;
        maximum[2] = prop->pos.z + width;
        entry = 0.0f;
        leave = 1.0f;
        axis = -1;
        side = 1.0f;
        for (j = 0; j < 3; j++)
        {
            if (delta.f[j] > -0.00001f && delta.f[j] < 0.00001f)
            {
                if (from->f[j] < minimum[j] || from->f[j] > maximum[j]) break;
            }
            else
            {
                near = (minimum[j] - from->f[j]) / delta.f[j];
                far = (maximum[j] - from->f[j]) / delta.f[j];
                if (near > far) { swap = near; near = far; far = swap; }
                if (near > entry)
                {
                    entry = near;
                    axis = j;
                    side = delta.f[j] > 0.0f ? -1.0f : 1.0f;
                }
                if (far < leave) leave = far;
                if (entry > leave) break;
            }
        }
        if (j != 3) continue;
        distance = length * entry;
        if (distance >= *bestdistance) continue;
        *bestdistance = distance;
        hit->x = from->x + delta.x * entry;
        hit->y = from->y + delta.y * entry;
        hit->z = from->z + delta.z * entry;
        normal->x = axis == 0 ? side : 0.0f;
        normal->y = axis < 0 ? 1.0f : (axis == 1 ? side : 0.0f);
        normal->z = axis == 2 ? side : 0.0f;
        closest = prop;
    }
    return closest;
}

s32 guardmodeTankProjectileCollision(PropRecord *projectile, ObjectRecord *target,
    coord3d *from, coord3d *to, coord3d *hit, coord3d *normal, f32 *bestdistance)
{
    ModelRoData_BoundingBoxRecord *box;
    ObjectRecord *round;
    Mtxf world;
    coord3d origin;
    coord3d delta;
    f32 start[3];
    f32 direction[3];
    f32 entry = 0.0f;
    f32 leave = 1.0f;
    f32 scale2;
    f32 near;
    f32 far;
    f32 swap;
    f32 distance;
    s32 axis = -1;
    s32 side = -1;
    s32 i;
    s32 tagged = FALSE;
    if (!guardmodeActive() || g_GuardMode.tank == NULL || projectile == NULL
        || target == NULL || target->prop == NULL || (target->state & PROPSTATE_20)) return FALSE;
    round = projectile->obj;
    if (round == NULL || !(round->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        || round->projectile->ownerprop != g_GuardMode.tank->prop) return FALSE;
    for (i = 0; i < 4; i++) if (target == objFindByTagId(objectivetags[i])) tagged = TRUE;
    if (!tagged) return FALSE;
    box = guardmodeObjectBoxNode(target) == NULL ? NULL : guardmodeObjectBoxNode(target)->Data;
    if (box == NULL) return FALSE;
    guardmodeObjectBoundsMatrix(target, &world);
    origin.x = from->x - world.m[3][0];
    origin.y = from->y - world.m[3][1];
    origin.z = from->z - world.m[3][2];
    delta.x = to->x - from->x;
    delta.y = to->y - from->y;
    delta.z = to->z - from->z;
    for (i = 0; i < 3; i++)
    {
        scale2 = world.m[i][0] * world.m[i][0]
            + world.m[i][1] * world.m[i][1]
            + world.m[i][2] * world.m[i][2];
        if (scale2 < 0.000001f) return FALSE;
        start[i] = (origin.x * world.m[i][0] + origin.y * world.m[i][1]
            + origin.z * world.m[i][2]) / scale2;
        direction[i] = (delta.x * world.m[i][0] + delta.y * world.m[i][1]
            + delta.z * world.m[i][2]) / scale2;
        if (direction[i] > -0.000001f && direction[i] < 0.000001f)
        {
            if (start[i] < box->Bounds.f[i][0] || start[i] > box->Bounds.f[i][1]) return FALSE;
        }
        else
        {
            near = (box->Bounds.f[i][0] - start[i]) / direction[i];
            far = (box->Bounds.f[i][1] - start[i]) / direction[i];
            if (near > far) { swap = near; near = far; far = swap; }
            if (near > entry) { entry = near; axis = i; side = direction[i] > 0.0f ? -1 : 1; }
            if (far < leave) leave = far;
            if (entry > leave) return FALSE;
        }
    }
    distance = sqrtf(delta.x*delta.x + delta.y*delta.y + delta.z*delta.z) * entry;
    if (distance >= *bestdistance) return FALSE;
    *bestdistance = distance;
    hit->x = from->x + delta.x * entry;
    hit->y = from->y + delta.y * entry;
    hit->z = from->z + delta.z * entry;
    normal->x = axis < 0 ? 0.0f : world.m[axis][0] * side;
    normal->y = axis < 0 ? 1.0f : world.m[axis][1] * side;
    normal->z = axis < 0 ? 0.0f : world.m[axis][2] * side;
    guNormalize(&normal->x, &normal->y, &normal->z);
    return TRUE;
}

static f32 guardmodeTankBallistics(coord3d *muzzle, coord3d *target, coord3d *velocity)
{
    f32 x = target->x - muzzle->x;
    f32 z = target->z - muzzle->z;
    f32 y = target->y - muzzle->y;
    f32 horizontal = sqrtf(x*x + z*z);
    f32 speed2 = g_TankShellSpeed * g_TankShellSpeed;
    f32 gravity = 0.27777779f; /* Same trapezoidal gravity as native tank rounds. */
    f32 discriminant = speed2 * speed2 - gravity * (gravity * horizontal * horizontal + 2.0f * y * speed2);
    f32 pitch;
    if (horizontal < 1.0f || discriminant < 0.0f) return -1.0f;
    pitch = atan2f(speed2 - sqrtf(discriminant), gravity * horizontal);
    if (pitch > 0.4363323f || pitch < -0.12f) return -1.0f;
    velocity->x = x / horizontal * cosf(pitch) * g_TankShellSpeed;
    velocity->z = z / horizontal * cosf(pitch) * g_TankShellSpeed;
    velocity->y = sinf(pitch) * g_TankShellSpeed;
    return pitch;
}

static PropRecord *guardmodeTankGuardTarget(coord3d *muzzle, coord3d *point, s32 cannon)
{
    TankRecord *tank = g_GuardMode.tank;
    PropRecord *choices[11];
    coord3d points[11];
    s32 ids[11];
    PropRecord *prop;
    ChrRecord *chr;
    StandTile *stan;
    coord3d candidate;
    coord3d velocity;
    f32 distance;
    s32 count = 0;
    s32 choice = -1;
    s32 id;
    s32 i;
    s32 head;
    s32 clear;
    for (i = 0; i < 11; i++)
    {
        if (i == 10)
        {
            if (!guardmodePlayerTargetable()) continue;
            prop = g_CurrentPlayer->prop;
            id = -1;
        }
        else
        {
            chr = chrFindByLiteralId(guardids[i]);
            if (taken[i] || !guardmodeAlive(chr)) continue;
            prop = chr->prop;
            id = chr->chrnum;
        }
        distance = guardmodeHorizontalDistance2(&tank->prop->pos, &prop->pos);
        /* The held gun covers the cannon's close-range blind spot. Both
         * player and NPC guards pass exactly the same range/sight rules. */
        if (cannon ? (distance < 350.0f * 350.0f || distance > 5000.0f * 5000.0f)
            : distance > 1200.0f * 1200.0f) continue;
        for (head = 0; head < 2; head++)
        {
            guardmodeBodyPoint(prop, &candidate, head);
            if (cannon && guardmodeTankBallistics(muzzle, &candidate, &velocity) == -1.0f) continue;
            stan = tank->prop->stan;
            if (cannon) sub_GAME_7F03D058(tank->prop, FALSE);
            clear = stanTestLineUnobstructed(&stan, muzzle->x, muzzle->z, candidate.x, candidate.z,
                CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE,
                muzzle->y, muzzle->y, candidate.y, candidate.y);
            if (cannon) sub_GAME_7F03D058(tank->prop, TRUE);
            if (!clear || stan != prop->stan) continue;
            choices[count] = prop;
            points[count] = candidate;
            ids[count++] = id;
            break;
        }
    }
    if (count == 0)
    {
        if (cannon) g_GuardMode.tankguardtarget = -2;
        else tankhandguntarget = -2;
        return NULL;
    }
    /* Hold a visible target while turning the turret. A recent, visible
     * attacker has priority; otherwise give every eligible guard a chance. */
    for (i = 0; i < count; i++)
        if (ids[i] == (cannon ? g_GuardMode.tankguardtarget : tankhandguntarget)) choice = i;
    if (g_GlobalTimer < g_GuardMode.retaliateuntil)
        for (i = 0; i < count; i++)
            if (ids[i] == g_GuardMode.retaliateid) choice = i;
    if (choice < 0) choice = randomGetNext() % count;
    if (cannon) g_GuardMode.tankguardtarget = ids[choice];
    else tankhandguntarget = ids[choice];
    *point = points[choice];
    return choices[choice];
}

static s32 guardmodeTankTrack(void)
{
    TankRecord *tank = g_GuardMode.tank;
    ObjectRecord *obj;
    ObjectRecord *bestobj = NULL;
    PropRecord *guard = NULL;
    WeaponObjRecord *shell;
    coord3d muzzle;
    coord3d target;
    coord3d bestpoint;
    coord3d velocity;
    StandTile *stan;
    Mtxf matrix;
    Mtxf identity;
    f32 distance;
    f32 best = 5500.0f * 5500.0f;
    f32 yaw;
    f32 pitch;
    f32 error;
    s32 clear;
    s32 i;
    s32 remaining = 0;
    g_GuardMode.objectivetarget = -1;
    guardmodeTankMuzzle(&muzzle);
    for (i = 0; i < 4; i++)
    {
        if (!(g_GuardMode.requiredmask & (1 << i)) || (g_GuardMode.destroyedmask & (1 << i))) continue;
        remaining++;
        /* Two unsuccessful rounds are a reason to find a different angle,
         * rather than emptying all thirty shells from the same blocked spot. */
        if (tanksalvos[i] >= 2)
        {
            if (guardmodeHorizontalDistance2(&tank->prop->pos, &tankfirepos[i]) < 750.0f * 750.0f) continue;
            tanksalvos[i] = 0;
        }
        obj = objFindByTagId(objectivetags[i]);
        if (obj == NULL || obj->prop == NULL || !objIsHealthy(obj)) continue;
        guardmodeObjectPoint(obj, &target);
        distance = guardmodeHorizontalDistance2(&tank->prop->pos, &target);
        if (distance >= best) continue;
        stan = tank->prop->stan;
        sub_GAME_7F03D058(tank->prop, FALSE);
        sub_GAME_7F03D058(obj->prop, FALSE);
        clear = stanTestLineUnobstructed(&stan, muzzle.x, muzzle.z, target.x, target.z,
            CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE,
            muzzle.y, muzzle.y, target.y, target.y);
        sub_GAME_7F03D058(obj->prop, TRUE);
        sub_GAME_7F03D058(tank->prop, TRUE);
        if (!clear) continue;
        best = distance;
        bestobj = obj;
        bestpoint = target;
        g_GuardMode.objectivetarget = i;
    }
    /* Defences retain the bulk of the ammunition and firing windows. Use a
     * spare window for a real cannon shot at a guard, then resume the route
     * or objective immediately rather than camping the same human target. */
    if (g_GlobalTimer >= tankguardnextshot && tank->unkD8 > remaining * 3)
    {
        guard = guardmodeTankGuardTarget(&muzzle, &target, TRUE);
        if (guard != NULL)
        {
            bestpoint = target;
            g_GuardMode.objectivetarget = -1;
        }
    }
    if (bestobj == NULL && guard == NULL) return FALSE;
    yaw = atan2f(tank->runtime_pos.x - bestpoint.x, bestpoint.z - tank->runtime_pos.z);
    pitch = guardmodeTankBallistics(&muzzle, &bestpoint, &velocity);
    if (pitch == -1.0f) return FALSE;
    tank->turret_orientation_angle = guardmodeTurnToward(tank->turret_orientation_angle,
        yaw - tankyaw, 0.025f * g_GlobalTimerDelta);
    tank->turret_vertical_angle = guardmodeTurnToward(tank->turret_vertical_angle,
        pitch, 0.02f * g_GlobalTimerDelta);
    error = guardmodeAngleDelta(yaw - tankyaw, tank->turret_orientation_angle);
    if (error < -0.025f || error > 0.025f || g_GlobalTimer < tanknextshot) return TRUE;
    if (tank->unkD8 <= 0) return TRUE;
    guardmodeTankMuzzle(&muzzle);
    if (guardmodeTankBallistics(&muzzle, &bestpoint, &velocity) == -1.0f) return FALSE;
    shell = (WeaponObjRecord *)create_new_item_instance_of_model(PROP_CHRROCKET, ITEM_ROCKETROUND);
    if (shell == NULL) return TRUE;
    matrix_4x4_set_identity(&matrix);
    matrix_4x4_set_identity(&identity);
    shell->timer = -1;
    gunInitProjectileObject((ObjectRecord *)shell, &muzzle, tank->prop->stan,
        &matrix, &velocity, &identity, tank->prop);
    if (shell->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE)
        shell->projectile->flags |= PROJECTILEFLAG_LAUNCHING;
    chrobjSndCreatePostEventDefault(sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,
        GUN_TANK2BIG_1_SFX, NULL), &tank->prop->pos);
    tank->is_firing_tank = TRUE;
    tanknextshot = g_GlobalTimer + 90;
    tank->unkD8--;
    g_GuardMode.tankshots++;
    if (guard != NULL)
    {
        g_GuardMode.tankguardshots++;
        g_GuardMode.tanklastguard = g_GuardMode.tankguardtarget;
        if (g_GuardMode.tankguardtarget == -1) g_GuardMode.tankplayercannonshots++;
        tankguardnextshot = g_GlobalTimer + 300;
        g_GuardMode.tankguardtarget = -2;
    }
    else
    {
        tanksalvos[g_GuardMode.objectivetarget]++;
        tankfirepos[g_GuardMode.objectivetarget] = tank->prop->pos;
    }
    return TRUE;
}

/* Match the retail driving footprint and native actor death handling. The
 * on-foot human is an enemy guard too; do not route them through Bond's
 * ordinary tank-entry/platform support code. Only actual movement crushes. */
s32 guardmodeTankGetBounds(rect4f *bounds)
{
    TankRecord *tank = g_GuardMode.tank;
    PropRecord *savedtank = g_PlayerTankProp;
    coord3d savedoffset = g_TankModelPositionOffset;
    if (!guardmodeBondInTank(g_GuardMode.bond) || tank == NULL || tank->prop == NULL)
        return FALSE;
    g_PlayerTankProp = tank->prop;
    g_TankModelPositionOffset.x = 0.0f;
    g_TankModelPositionOffset.y = 0.0f;
    g_TankModelPositionOffset.z = 0.0f;
    bondviewGetTankCollisionBounds(bounds, &tank->prop->pos, tankyaw);
    g_TankModelPositionOffset = savedoffset;
    g_PlayerTankProp = savedtank;
    return TRUE;
}

static void guardmodeTankCrush(void)
{
    TankRecord *tank = g_GuardMode.tank;
    rect4f bounds;
    coord3d origin = tank->prop->pos;
    PropRecord *prop;
    ChrRecord *chr;
    ChrRecord *savedshooter = g_GuardModeShooter;
    f32 radius;
    f32 floor;
    s32 i;
    if (!guardmodeTankGetBounds(&bounds)) return;
    for (i = 0; i < 11; i++)
    {
        if (i == 10)
        {
            if (!guardmodePlayerTargetable()) continue;
            prop = g_CurrentPlayer->prop;
            radius = g_CurrentPlayer->field_488.collision_radius;
            floor = g_CurrentPlayer->stanHeight;
        }
        else
        {
            chr = chrFindByLiteralId(guardids[i]);
            if (taken[i] || !guardmodeAlive(chr)) continue;
            prop = chr->prop;
            radius = chr->chrwidth;
            floor = chr->ground;
        }
        /* A matching X/Z on a different floor is not a track impact. */
        if (floor < tank->stan_y - 100.0f || floor > tank->stan_y + 100.0f) continue;
        if (!chrpropTestPointInPolygon(&prop->pos, &bounds, 4)
            && !chrobjTestPointPolygonCollision(&prop->pos, radius, &bounds, 4)) continue;
        g_GuardModeShooter = g_GuardMode.bond;
        chrobjSndCreatePostEventDefault(sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,
            TANK_CRUSH_MAN_SFX, NULL), &prop->pos);
        if (i == 10)
        {
            /* Crushing is a lethal physical hazard, not another Bond bullet.
             * The normal guard damage hook intentionally reduces every Bond
             * bullet to one square and would turn this into a harmless shove. */
            bondviewKillCurrentPlayer();
            if (g_CurrentPlayer->bonddead)
            {
                g_CurrentPlayer->bondhealth = 0.0f;
                g_CurrentPlayer->bondarmour = 0.0f;
                g_GuardMode.tankplayercrushes++;
                g_GuardMode.tankcrushdeath = TRUE;
            }
        }
        else
        {
            origin.y = prop->pos.y;
            chrlvExplosionDamage(chr, &origin, 3.0f, 1);
            if (chrIsDead(chr)) g_GuardMode.tankguardcrushes++;
        }
        g_GuardModeShooter = savedshooter;
    }
}

/* Use the same authored hatch offset, crouched body pose and deck-minus37
 * support height as the native third-person tank driver. Feet remain inside
 * the hull while the upper body is visible from a guard's viewpoint. */
static void guardmodeTankDriverPose(void)
{
    TankRecord *tank = g_GuardMode.tank;
    ChrRecord *chr = g_GuardMode.bond;
    struct firing_anim_struct *pose = &firing_animation_groups[
        g_GuardMode.bondweapon <= ITEM_TT33 ? 0 : 1][5];
    ModelAnimation *anim = (ModelAnimation *)*(s32 *)pose->pointer;
    coord3d seat = *(coord3d *)tank->model->obj->Switches[2]->Data;
    coord3d *base = tank->model->obj->Switches[1]->Data;
    f32 *hull = tank->model->obj->Switches[5]->Data;
    f32 *turret = tank->model->obj->Switches[6]->Data;
    Mtxf rotation;
    f32 floor = tank->stan_y + ((hull[4] - hull[3]) + (turret[4] - turret[3]))
        * tank->model->scale - 37.0f;
    matrix_4x4_set_rotation_around_y(M_TAU_F - tank->turret_orientation_angle, &rotation);
    mtx4RotateVecInPlace(&rotation, &seat);
    seat.x += base->x;
    seat.y += base->y;
    seat.z += base->z;
    mtx4RotateVecInPlace(&tank->mtx, &seat);
    seat.x += tank->runtime_pos.x;
    seat.z += tank->runtime_pos.z;
    seat.y = floor + 100.0f;
    chr->prop->pos = seat;
    chr->prop->stan = tank->prop->stan;
    chr->ground = floor;
    chr->manground = floor;
    chr->sumground = floor / 0.17000002f;
    chr->prevpos = seat;
    chr->chrflags |= CHRFLAG_LOCK_Y_POS;
    if (objecthandlerGetModelAnim(chr->model) != anim)
    {
        modelSetAnimation(chr->model, anim, 0, pose->y, pose->x, 0.0f);
        modelSetAnimLooping(chr->model, pose->y, 0.0f);
        modelSetAnimEndFrame(chr->model, pose->z);
    }
    setsuboffset(chr->model, &seat);
    setsubroty(chr->model, M_TAU_F - tankyaw - tank->turret_orientation_angle);
    chr->aimendback = 0.0f;
    chr->aimendsideback = 0.0f;
    chrlvUpdateAimendbackShoulders(chr, pose->pointer, FALSE, TRUE, 0.0f);
}

/* A human Bond can use a held gun from the hatch as well as the cannon.
 * Reuse the existing native bullet trace, magazine, aim spread, hit sound
 * and reload path for nearby guards that the cannon cannot cover. */
static void guardmodeTankHeldGunTick(void)
{
    ChrRecord *chr = g_GuardMode.bond;
    PropRecord *target;
    coord3d muzzle;
    coord3d point;
    s32 before;
    f32 horizontal;
    f32 pitch;
    struct firing_anim_struct *pose = &firing_animation_groups[
        g_GuardMode.bondweapon <= ITEM_TT33 ? 0 : 1][5];
    chrSetFiring(chr, GUNRIGHT, FALSE);
    if (!sub_GAME_7F02D630(chr, GUNRIGHT, &muzzle))
    {
        muzzle = chr->prop->pos;
        muzzle.y += 30.0f;
    }
    target = guardmodeTankGuardTarget(&muzzle, &point, FALSE);
    if (target == NULL) return;
    g_GuardMode.targetid = tankhandguntarget;
    chr->seen_bond_time = g_GlobalTimer;
    setsubroty(chr->model, atan2f(point.x - chr->prop->pos.x, point.z - chr->prop->pos.z));
    horizontal = sqrtf(guardmodeHorizontalDistance2(&muzzle, &point));
    pitch = atan2f(point.y - muzzle.y, horizontal);
    chrlvUpdateAimendbackShoulders(chr, pose->pointer, FALSE, TRUE, pitch);
    before = g_GuardMode.shots;
    tankhandgunfiring = TRUE;
    chrlvFireWeaponRelated(chr, GUNRIGHT);
    tankhandgunfiring = FALSE;
    if (g_GuardMode.shots > before)
    {
        g_GuardMode.tankhandgunshots++;
        if (tankhandguntarget == -1) g_GuardMode.tankplayerhandgunshots++;
        /* Keep close defence deliberately gentler than on-foot gunfire. */
        g_GuardMode.nextshot = g_GlobalTimer + 45;
    }
}

static void guardmodeTankDrive(coord3d *goal)
{
    TankRecord *tank = g_GuardMode.tank;
    coord3d next = tank->prop->pos;
    StandTile *stan;
    Mtxf rotation;
    f32 yaw = atan2f(next.x - goal->x, goal->z - next.z);
    f32 heading;
    f32 turn;
    f32 move;
    s32 attempt;
    s32 moved = FALSE;
    tankyaw = guardmodeTurnToward(tankyaw, yaw, 0.018f * g_GlobalTimerDelta);
    /* Turning through a half-circle at full speed drove the parked tank
     * into the eastern container wall. Align in place before accelerating;
     * use the native full footprint for rotation as well as translation. */
    turn = guardmodeAngleDelta(yaw, tankyaw);
    if (turn < -0.6f || turn > 0.6f) tankspeed = 0.0f;
    else tankspeed += 0.1f * g_GlobalTimerDelta;
    if (tankspeed > 8.0f) tankspeed = 8.0f;
    move = tankspeed * g_GlobalTimerDelta;
    for (attempt = 0; attempt < 5 && !moved; attempt++)
    {
        turn = attempt == 0 ? 0.0f : (attempt == 1 ? 0.35f : (attempt == 2 ? -0.35f : (attempt == 3 ? 0.7f : -0.7f)));
        heading = tankyaw + turn;
        next = tank->prop->pos;
        next.x -= sinf(heading) * move;
        next.z += cosf(heading) * move;
        stan = tank->prop->stan;
        if (walkTilesBetweenPoints_NoCallback(&stan, tank->prop->pos.x, tank->prop->pos.z, next.x, next.z)
            && guardmodeTankCanMove(&next, stan, heading))
        {
            tankyaw = heading;
            tank->stan_y = stanGetPositionYValue(stan, next.x, next.z);
            next.y = tank->stan_y + tankheight;
            matrix_4x4_set_rotation_around_y(M_TAU_F - tankyaw, &rotation);
            matrix_scalar_multiply(tank->model->scale, rotation.m[0]);
            objChangeShading((ObjectRecord *)tank, &next, &rotation, stan);
            tank->tank_orientation_angle = tankyaw;
            setupUpdateObjectRoomPosition((ObjectRecord *)tank);
            chrobjCollisionRelated((ObjectRecord *)tank);
            guardmodeTankCrush();
            moved = TRUE;
        }
    }
    if (!moved) tankspeed = 0.0f;
}

void guardmodeVehicleFrame(void)
{
    TankRecord *tank = g_GuardMode.tank;
    ChrRecord *chr = g_GuardMode.bond;
    coord3d exitpos;
    StandTile *stan;
    coord3d *goal;
    f32 oldx;
    f32 oldz;
    s32 i;
    s32 tracked;
    if (!guardmodeActive() || !g_GuardMode.initialized || tank == NULL || tank->prop == NULL
        || g_ClockTimer <= 0 || tanklastframe == g_GlobalTimer) return;
    tanklastframe = g_GlobalTimer;
    if (g_GuardMode.result != 0 || chrIsDead(chr) || g_GuardMode.tankphase >= 5)
    {
        if (tankengine != NULL && sndGetPlayingState(tankengine)) sndDeactivate(tankengine);
        tankengine = NULL;
        tank->is_firing_tank = FALSE;
        return;
    }
    if (g_GuardMode.tankphase == 2 && g_GlobalTimer - tankat >= 60)
    {
        g_GuardMode.tankphase = 3;
        g_GuardMode.tankentered++;
        tank->unkD8 = 30;
        tanknextshot = g_GlobalTimer + 30;
        tankguardnextshot = g_GlobalTimer + 180;
        check_set_actor_standing_still(chr, TARGET_COMPASS, 0);
        hudmsgTopShow("BOND ENTERED THE TANK");
    }
    if (!guardmodeBondInTank(chr)) return;
    guardmodeObjectiveMaskTick();
    if (g_GuardMode.tankphase == 3)
    {
        if (tankengine == NULL) sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, TANK_SFX, &tankengine);
        if (tankengine != NULL) chrobjSndCreatePostEventDefault(tankengine, &tank->prop->pos);
        tank->is_firing_tank = g_GlobalTimer >= tanknextshot - 90 && g_GlobalTimer < tanknextshot - 84;
        tracked = guardmodeTankTrack();
        if (!tracked)
        {
            goal = &g_CurrentSetup.pads[tankpath[g_GuardMode.tankwaypoint]].pos;
            if (guardmodeHorizontalDistance2(&tank->prop->pos, goal) < 450.0f * 450.0f
                && g_GuardMode.tankwaypoint < 9) g_GuardMode.tankwaypoint++;
            else if (g_GuardMode.tankwaypoint == 9
                && guardmodeHorizontalDistance2(&tank->prop->pos, goal) < 450.0f * 450.0f)
                g_GuardMode.tankwaypoint = 6;
            goal = &g_CurrentSetup.pads[tankpath[g_GuardMode.tankwaypoint]].pos;
            guardmodeTankDrive(goal);
        }
        else tankspeed = 0.0f;
        if ((g_GuardMode.destroyedmask & g_GuardMode.requiredmask) == g_GuardMode.requiredmask)
        {
            g_GuardMode.tankphase = 4;
            tankat = g_GlobalTimer;
            tank->is_firing_tank = FALSE;
            hudmsgTopShow("BOND CLEARED THE AIRCRAFT DEFENCES");
        }
    }
    guardmodeTankDriverPose();
    if (g_GuardMode.tankphase == 3) guardmodeTankHeldGunTick();
    if (g_GuardMode.tankphase == 4 && g_GlobalTimer - tankat >= 60)
    {
        for (i = 0; i < 4; i++)
        {
            oldx = sinf(tankyaw + M_PI_2F * i) * 650.0f;
            oldz = cosf(tankyaw + M_PI_2F * i) * 650.0f;
            exitpos = tank->prop->pos;
            exitpos.x += oldx;
            exitpos.z += oldz;
            stan = tank->prop->stan;
            if (walkTilesBetweenPoints_NoCallback(&stan, tank->prop->pos.x, tank->prop->pos.z, exitpos.x, exitpos.z))
            {
                exitpos.y = stanGetPositionYValue(stan, exitpos.x, exitpos.z) + 100.0f;
                chr->prop->stan = stan;
                chr->prop->pos = exitpos;
                chr->prevpos = exitpos;
                chr->ground = exitpos.y - 100.0f;
                chr->manground = chr->ground;
                chr->sumground = chr->ground / 0.17000002f;
                setsuboffset(chr->model, &exitpos);
                chr->chrflags &= ~CHRFLAG_LOCK_Y_POS;
                g_GuardMode.tankend = tank->prop->pos;
                g_GuardMode.tankphase = 5;
                g_GuardMode.route = 12;
                routecommand = -1;
                navigationuntil = 0;
                progressat = 0;
                check_set_actor_standing_still(chr, TARGET_COMPASS, 0);
                break;
            }
        }
    }
}
