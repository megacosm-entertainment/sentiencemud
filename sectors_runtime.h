#ifndef SECTORS_RUNTIME_H
#define SECTORS_RUNTIME_H

#define SECTOR_NO_MAGIC      (A)
#define SECTOR_HARD_MAGIC    (B)
#define SECTOR_SLOW_MAGIC    (C)
#define SECTOR_DEEP_WATER    (D)
#define SECTOR_UNDERWATER    (E)
#define SECTOR_FLAME         (F)
#define SECTOR_FROZEN        (G)
#define SECTOR_AERIAL        (H)
#define SECTOR_INDOORS       (I)
#define SECTOR_BRIARS        (J)
#define SECTOR_TOXIC         (K)
#define SECTOR_NO_SOIL       (L)
#define SECTOR_CRUMBLES      (M)
#define SECTOR_MELTS         (N)
#define SECTOR_NO_HIDE_OBJ   (O)
#define SECTOR_NO_FADE       (P)
#define SECTOR_NO_GOHALL     (Q)
#define SECTOR_CITY_LIGHTS   (R)
#define SECTOR_NO_GATE       (S)
#define SECTOR_SLEEP_DRAIN   (T)
#define SECTOR_DRAIN_MANA    (U)
#define SECTOR_NATURE        (V)

#define SECTOR_MAX_AFFINITIES 3
#define SECTOR_MAX_HIDE_MSGS 8

typedef struct sector_affinity_data SECTOR_AFFINITY_DATA;
typedef struct sector_runtime_data SECTOR_RUNTIME_DATA;

struct sector_affinity_data {
    int catalyst;
    int value;
};

struct sector_runtime_data {
    int id;
    char *name;
    char *description;
    int move_cost;
    int heal_rate;
    int mana_rate;
    int move_rate;
    int sector_class;
    long flags;
    int soil;
    char *comments;
    char *hide_msgs[SECTOR_MAX_HIDE_MSGS];
    SECTOR_AFFINITY_DATA affinities[SECTOR_MAX_AFFINITIES];
};

#endif