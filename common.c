/* common.c - shared data and helper functions (see common.h) */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

FloorRoom citFloor[MAX_ROOMS];     /* the rooms */
int       roomCount = 0;           /* number of rooms */

ClassSlot timetable[MAX_SLOTS];    /* every scheduled class */
int       slotCount = 0;           /* number of scheduled classes */

void clearInputTrail(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) { }
}

/* Mon, Tue, Wed, Thu, Fri, Sat (any letter case) -> 1 to 6, or 0 if not a day */
int dayToKey(const char *day) {
    char tmp[MAX_DAY_LEN];
    int i;
    for (i = 0; day[i] && i < MAX_DAY_LEN - 1; i++)
        tmp[i] = (char)tolower((unsigned char)day[i]);
    tmp[i] = '\0';
    if (strncmp(tmp, "mon", 3) == 0) return 1;
    if (strncmp(tmp, "tue", 3) == 0) return 2;
    if (strncmp(tmp, "wed", 3) == 0) return 3;
    if (strncmp(tmp, "thu", 3) == 0) return 4;
    if (strncmp(tmp, "fri", 3) == 0) return 5;
    if (strncmp(tmp, "sat", 3) == 0) return 6;
    return 0;
}

/* accepts "7:30", "07:30", "730" or "1330" and gives back minutes from 00:00 */
int parseClock(const char *raw, int *outMins) {
    int h = 0, m = 0;
    char copy[32];
    strncpy(copy, raw, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    if (strchr(copy, ':') != NULL) {
        if (sscanf(copy, "%d:%d", &h, &m) != 2) return 0;
    } else {
        int digits = (int)strlen(copy);
        if (digits < 3 || digits > 4) return 0;
        if (digits == 3) { h = copy[0] - '0'; m = (copy[1] - '0') * 10 + (copy[2] - '0'); }
        else { h = (copy[0] - '0') * 10 + (copy[1] - '0'); m = (copy[2] - '0') * 10 + (copy[3] - '0'); }
    }
    if (h < 0 || h > 23 || m < 0 || m > 59) return 0;
    *outMins = h * 60 + m;
    return 1;
}

/* [start, end) half-open: a class that ends at 9:00 and one that starts at 9:00 do not conflict */
int timesOverlap(int aStart, int aEnd, int bStart, int bEnd) {
    return (aStart < bEnd) && (bStart < aEnd);
}

/* returns the room with this tag, or NULL if there is none */
FloorRoom *findRoom(const char *tag) {
    int i;
    for (i = 0; i < roomCount; i++) {
        if (strcmp(citFloor[i].roomTag, tag) == 0)
            return &citFloor[i];
    }
    return NULL;
}

/* ---------- Room list: defaults, and loading from rooms.txt ---------- */

/* adds one room at the end of the list (used for the default rooms) */
static void setRoom(const char *tag, const char *type, int capacity, int usable) {
    snprintf(citFloor[roomCount].roomTag,  sizeof(citFloor[roomCount].roomTag),  "%s", tag);
    snprintf(citFloor[roomCount].roomType, sizeof(citFloor[roomCount].roomType), "%s", type);
    citFloor[roomCount].seatLimit = capacity;
    citFloor[roomCount].isUsable = usable;
    roomCount++;
}

/* CIT 4th-floor rooms, used when there is no saved file yet */
static void loadDefaultRooms(void) {
    roomCount = 0;
    setRoom("CIT-400", "storage", 0, 0);
    setRoom("CIT-401", "faculty", 0, 0);
    setRoom("CIT-402", "lab", 25, 1);
    setRoom("CIT-403", "lab", 25, 1);
    setRoom("CIT-404", "lab", 25, 1);
    setRoom("CIT-405", "lab", 25, 1);
    setRoom("CIT-406", "lab", 0, 0);       /* no capacity and not in use (can be edited later) */
    setRoom("CIT-407", "lab", 25, 1);
    setRoom("CIT-408", "lab", 25, 1);
    setRoom("CIT-409", "lab", 25, 1);
    setRoom("CIT-410", "lab", 25, 1);
    setRoom("CIT-411", "storage", 0, 0);
    setRoom("CIT-412", "lab", 25, 1);
    setRoom("CIT-417", "lab", 25, 1);
    setRoom("CIT-418", "lab", 25, 1);
    setRoom("CIT-419", "lab", 25, 1);
    setRoom("CIT-420", "lab", 25, 1);
    setRoom("CIT-421", "lab", 25, 1);
    setRoom("CIT-422", "lab", 25, 1);
    setRoom("CIT-423", "lec", 45, 1);
    setRoom("CIT-424", "lab", 25, 1);
    setRoom("CIT-425", "lab", 25, 1);
    setRoom("CIT-426", "lab", 25, 1);
}

/* Reads the whole file into temp[]. Returns 1 only if EVERYTHING in it is valid,
   so a damaged file can never leave half-loaded rooms behind. */
static int readRoomsFile(FILE *fp, FloorRoom temp[], int *count) {
    int i, j;

    if (fscanf(fp, "%d", count) != 1 || *count < 0 || *count > MAX_ROOMS) {
        return 0;
    }
    for (i = 0; i < *count; i++) {
        if (fscanf(fp, "%23s %15s %d %d", temp[i].roomTag, temp[i].roomType,
                   &temp[i].seatLimit, &temp[i].isUsable) != 4) {
            return 0;
        }
        if (temp[i].seatLimit < 0 || (temp[i].isUsable != 0 && temp[i].isUsable != 1)) {
            return 0;
        }
        for (j = 0; j < i; j++) {                  /* every room tag must be unique */
            if (strcmp(temp[j].roomTag, temp[i].roomTag) == 0) {
                return 0;
            }
        }
    }
    return 1;
}

/* delete rooms.txt to go back to the default rooms */
void loadRooms(void) {
    FILE *fp;
    FloorRoom temp[MAX_ROOMS];
    int count = 0, i, ok;

    fp = fopen(ROOMS_FILE, "r");
    if (fp == NULL) {
        loadDefaultRooms();
        printf("No saved file yet. Loaded the default %d rooms.\n", roomCount);
        return;
    }

    ok = readRoomsFile(fp, temp, &count);
    fclose(fp);

    if (!ok) {
        loadDefaultRooms();
        printf("Saved file is not valid. Loaded the default %d rooms.\n", roomCount);
        return;
    }

    for (i = 0; i < count; i++) {
        citFloor[i] = temp[i];
    }
    roomCount = count;
    printf("Loaded %d room(s).\n", roomCount);
}

/* ---------- Schedule: saving and loading (schedule.txt) ----------
 *
 * File format (every value on its own line, so titles can contain spaces):
 *   SCHEDULE_V1
 *   <number of classes>
 *   then, for each class:
 *     <subject code>
 *     <subject title>
 *     <day>
 *     <room tag>
 *     <start minutes> <end minutes> <class size>
 */

/* Reads one line into dest (size bytes). Returns 0 if there is no line or it is too long. */
static int readTextLine(FILE *fp, char *dest, size_t size) {
    char line[128];
    size_t len;

    if (fgets(line, sizeof line, fp) == NULL) {
        return 0;
    }
    len = strcspn(line, "\r\n");
    if (line[len] == '\0' && len >= sizeof line - 1) {
        return 0;                          /* line did not fit: not a valid file */
    }
    if (len >= size) {
        return 0;                          /* value is longer than its field */
    }
    memcpy(dest, line, len);
    dest[len] = '\0';
    return 1;
}

/* Reads the whole file into temp[]. Returns 1 only if EVERYTHING in it is valid. */
static int readScheduleFile(FILE *fp, ClassSlot temp[], int *count) {
    char line[128];
    int i;

    if (!readTextLine(fp, line, sizeof line) || strcmp(line, SCHEDULE_HEADER) != 0) {
        return 0;
    }
    if (!readTextLine(fp, line, sizeof line) ||
        sscanf(line, "%d", count) != 1 || *count < 0 || *count > MAX_SLOTS) {
        return 0;
    }

    for (i = 0; i < *count; i++) {
        ClassSlot *c = &temp[i];

        memset(c, 0, sizeof *c);
        if (!readTextLine(fp, c->subjCode,  sizeof c->subjCode))  return 0;
        if (!readTextLine(fp, c->subjTitle, sizeof c->subjTitle)) return 0;
        if (!readTextLine(fp, c->weekDay,   sizeof c->weekDay))   return 0;
        if (!readTextLine(fp, c->roomTag,   sizeof c->roomTag))   return 0;

        if (!readTextLine(fp, line, sizeof line) ||
            sscanf(line, "%d %d %d", &c->startMins, &c->endMins, &c->headCount) != 3) {
            return 0;
        }
        if (dayToKey(c->weekDay) == 0 ||
            c->startMins < 0 || c->endMins > 24 * 60 - 1 || c->endMins <= c->startMins ||
            c->headCount <= 0) {
            return 0;
        }
    }
    return 1;
}

/* Returns 1 on success, 0 on failure */
int saveSchedule(void) {
    FILE *fp = fopen(SCHEDULE_FILE, "w");
    int i;

    if (fp == NULL) {
        printf("Cannot save the schedule file.\n");
        return 0;
    }

    fprintf(fp, "%s\n%d\n", SCHEDULE_HEADER, slotCount);
    for (i = 0; i < slotCount; i++) {
        fprintf(fp, "%s\n%s\n%s\n%s\n%d %d %d\n",
                timetable[i].subjCode, timetable[i].subjTitle,
                timetable[i].weekDay, timetable[i].roomTag,
                timetable[i].startMins, timetable[i].endMins, timetable[i].headCount);
    }

    if (ferror(fp) || fclose(fp) != 0) {
        printf("Error while writing the schedule file.\n");
        return 0;
    }
    printf("Saved to %s\n", SCHEDULE_FILE);
    return 1;
}

/* delete schedule.txt to start with an empty schedule */
void loadSchedule(void) {
    FILE *fp;
    ClassSlot temp[MAX_SLOTS];
    int count = 0, i, ok;

    fp = fopen(SCHEDULE_FILE, "r");
    if (fp == NULL) {
        slotCount = 0;
        printf("No saved schedule yet. Starting with an empty schedule.\n");
        return;
    }

    ok = readScheduleFile(fp, temp, &count);
    fclose(fp);

    if (!ok) {
        slotCount = 0;
        printf("Saved schedule file is not valid. Starting with an empty schedule.\n");
        return;
    }

    for (i = 0; i < count; i++) {
        timetable[i] = temp[i];
    }
    slotCount = count;
    printf("Loaded %d scheduled class(es).\n", slotCount);
}

/* ---------- Room list: saving (rooms.txt) ---------- */

int roomsChanged = 0;              /* 1 = room changes that are not saved yet */

/* Returns 1 on success, 0 on failure */
int saveRooms(void) {
    FILE *fp;
    int i;

    fp = fopen(ROOMS_FILE, "w");
    if (fp == NULL) {
        printf("Cannot save file.\n");
        return 0;
    }

    fprintf(fp, "%d\n", roomCount);
    for (i = 0; i < roomCount; i++) {
        fprintf(fp, "%s %s %d %d\n", citFloor[i].roomTag, citFloor[i].roomType,
                citFloor[i].seatLimit, citFloor[i].isUsable);
    }

    if (ferror(fp) || fclose(fp) != 0) {
        printf("Error while writing the file.\n");
        return 0;
    }

    roomsChanged = 0;
    printf("Saved to %s\n", ROOMS_FILE);
    return 1;
}
