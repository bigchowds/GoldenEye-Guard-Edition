#ifndef GE_GUARDMODE_H
#define GE_GUARDMODE_H
#include <ultra64.h>
#include <bondtypes.h>

/* Runway-only counter-operative prototype. No retail structure is enlarged. */
#define GUARDMODE_BOND_ID 150
#define GUARDMODE_WIN_FLAG 0x10000000
#define GUARDMODE_FAIL_FLAG 0x20000000

typedef struct GuardModeState {
    s32 active;
    s32 initialized;
    s32 currentguard;
    s32 body;
    s32 head;
    s32 deaths;
    s32 remaining;
    s32 route;
    s32 result; /* 0 playing, 1 Bond killed, 2 Bond escaped, 3 no guards */
    s32 deathat;
    s32 finishat;
    s32 lastframe;
    s32 introcount;
    s32 aisteps;
    s32 shots;
    s32 hits;
    f32 bondhealth;
    ChrRecord *bond;
    s32 targetid; /* -2 none, -1 controlled guard, otherwise literal chr ID */
    s32 targetuntil;
    s32 clip;
    s32 reloadat;
    s32 reloads;
    s32 keytaken;
    coord3d spawnpos;
    s32 keyroomentered;
    coord3d keypickuppos;
    s32 keypickuproom;
    s32 strafes;
    DoorRecord *keydoor;
    s32 dooropens;
    s32 retaliateid;
    s32 retaliateuntil;
    s32 retaliations;
    s32 eligiblemask;
    s32 combatstarted;
    s32 nextshot;
    s32 droneshots;
    s32 dronehits;
    s32 bondweapon;
    s32 weaponpickups;
    PropRecord *loottarget;
    s32 playerdrops;
    s32 guardshots[10];
    s32 guardalerts[10];
    s32 difficulty;
    TankRecord *tank;
    s32 tankphase; /* 0 on foot, 1 approach, 2 board, 3 drive, 4 exit, 5 finished */
    s32 tankentered;
    s32 tankshots;
    s32 requiredmask; /* battery=1, heavy emplacements=2,4,8 */
    s32 destroyedmask;
    s32 objectivetarget;
    s32 tankwaypoint;
    coord3d tankstart;
    coord3d tankend;
    s32 guardhits[10]; /* Actual bullet damage, not just firing animations. */
    f32 guarddamage[10];
    s32 tankbullethits;
    s32 tankguardshots;
    s32 tankguardtarget; /* -2 none, -1 controlled guard, otherwise chr ID */
    s32 deathcamera; /* 0 pending, 1 one native collapse, 2 finished */
    s32 deathcamerastarts;
    s32 deathcamerafinishes;
    s32 deathcamerastart;
    s32 tanklastguard; /* Target of the most recent real anti-guard shell. */
    s32 tankguardimpacts;
    s32 deathbloodframes;
    s32 playerhurtsounds;
    s32 lastpainsfx;
    s32 tankhandgunshots;
    s32 tankplayercannonshots;
    s32 tankplayerhandgunshots;
    s32 tankplayercrushes;
    s32 tankguardcrushes;
    s32 tankcrushdeath;
} GuardModeState;

extern GuardModeState g_GuardMode;
extern ChrRecord *g_GuardModeShooter;
s32 guardmodeActive(void);
void guardmodeReset(s32 stage);
void guardmodeStageReady(void);
void guardmodeFrame(void);
s32 guardmodeAi(ChrRecord *chr);
void guardmodePlayerDied(void);
void guardmodeDeathCameraFinished(void);
void bondviewGuardDeathCameraStart(void);
s32 guardmodePlayerHitSound(void);
s32 guardmodeAllowDamage(ChrRecord *victim, s32 playerhit);
s32 guardmodeAllowAutoAim(ChrRecord *chr);
void guardmodePlayerAppearance(s32 *body, s32 *head);
s32 guardmodeIsBond(ChrRecord *chr);
s32 guardmodeBondCanStagger(ChrRecord *chr);
PropRecord *guardmodeTarget(ChrRecord *chr);
void guardmodeAttackTarget(ChrRecord *chr, u32 *type, s32 *id);
s32 guardmodeAimDirection(ChrRecord *chr, coord3d *origin, coord3d *direction);
s32 guardmodePlayerInBulletPath(coord3d *origin, coord3d *direction);
void guardmodeNotifyHit(ChrRecord *chr, s32 playerhit);
coord3d *guardmodeMoveTarget(ChrRecord *chr);
s32 guardmodeCanFire(ChrRecord *chr);
void guardmodeShotFired(ChrRecord *chr);
PropRecord *guardmodeDroneTarget(void);
void guardmodeDroneHit(coord3d *origin);
f32 guardmodePlayerDamageScale(void);
void guardmodeRecordDamage(ChrRecord *victim, f32 damage);
s32 guardmodeTankHit(ObjectRecord *obj, s32 weapon, coord3d *origin, s32 playerhit);
void guardmodeRecordKill(ChrRecord *chr);
void guardmodeVehicleFrame(void);
s32 guardmodeBondInTank(ChrRecord *chr);
s32 guardmodeTankGetBounds(rect4f *bounds);
s32 guardmodeTankProjectileCollision(PropRecord *projectile, ObjectRecord *target,
    coord3d *from, coord3d *to, coord3d *hit, coord3d *normal, f32 *bestdistance);
s32 guardmodeTankProjectileIgnores(PropRecord *projectile, PropRecord *candidate);
PropRecord *guardmodeTankCharacterCollision(PropRecord *projectile, coord3d *from,
    coord3d *to, coord3d *hit, coord3d *normal, f32 *bestdistance);
#endif
