/* common.h - shared definitions for the Room Capacity and Schedule modules
 *
 * Everything that BOTH modules need lives here and in common.c, written once:
 *   - the size limits
 *   - the FloorRoom and ClassSlot structures
 *   - the room list (citFloor / roomCount) and the class list (timetable / slotCount)
 *   - the helper functions for days, times and finding a room
 *
 * Both modules must #include "common.h" and be compiled together with common.c.
 */

#ifndef COMMON_H
#define COMMON_H

#define MAX_ROOMS    30
#define MAX_ROOM_LEN 24
#define MAX_SLOTS    80
#define MAX_CODE_LEN 16
#define MAX_NAME_LEN 48
#define MAX_DAY_LEN  12

#define ROOMS_FILE   "rooms.txt"   /* the saved room list, used by both modules */
#define SCHEDULE_FILE   "schedule.txt"   /* the saved class schedule */
#define SCHEDULE_HEADER "SCHEDULE_V1"    /* first line of schedule.txt */

typedef struct {
    char roomTag[MAX_ROOM_LEN];    /* room ID / name, e.g. CIT-401 */
    int  seatLimit;                /* capacity */
    int  isUsable;                 /* 1 = usable / available for scheduling, 0 = closed */
    char roomType[16];             /* lab, lec, faculty or storage */
} FloorRoom;

typedef struct {
    char subjCode[MAX_CODE_LEN];
    char subjTitle[MAX_NAME_LEN];
    char weekDay[MAX_DAY_LEN];     /* Mon Tue Wed Thu Fri Sat */
    int  startMins;                /* minutes from 00:00 */
    int  endMins;
    char roomTag[MAX_ROOM_LEN];
    int  headCount;                /* expected attendees / class size */
} ClassSlot;

/* shared data (the real variables are in common.c) */
extern FloorRoom citFloor[MAX_ROOMS];   /* the rooms */
extern int       roomCount;             /* number of rooms */

extern ClassSlot timetable[MAX_SLOTS];  /* every scheduled class */
extern int       slotCount;             /* number of scheduled classes */

/* loads the room list from ROOMS_FILE, or the default rooms if there is no valid file */
void       loadRooms(void);
int        saveRooms(void);        /* returns 1 on success, 0 on failure */
extern int roomsChanged;           /* 1 = room changes that are not saved yet */

/* saves / loads the class timetable to / from SCHEDULE_FILE */
int        saveSchedule(void);     /* returns 1 on success, 0 on failure */
void       loadSchedule(void);     /* starts empty if there is no valid file */

/* shared helper functions */
void       clearInputTrail(void);
int        dayToKey(const char *day);
int        parseClock(const char *raw, int *outMins);
int        timesOverlap(int aStart, int aEnd, int bStart, int bEnd);
FloorRoom *findRoom(const char *tag);

#endif
