#ifndef SECTORS_RUNTIME_H
#define SECTORS_RUNTIME_H

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