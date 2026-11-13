/* Features:
 *  - Add / list / delete class schedules
 *  - Detect overlapping time conflicts (same day)
 *  - Detect room double-booking conflicts
 *  - Check class size against room capacity
 *  - List rooms that fit a given class size
 *  - Suggest best-fit room (smallest room that still fits)
 *  - Rooms come from rooms.txt (shared with the Room Capacity Module);
 *    room edits made here are saved to rooms.txt immediately
 *  - The schedule is saved to / loaded from schedule.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "common.h"   /* shared structs, limits, room list, timetable and helpers */

static int scheduleChanged = 0;   /* 1 = changes that are not saved yet */

/* ---------- Helpers ---------- */

int readLineTrim(char *buf, int cap) {
    if (fgets(buf, cap, stdin) == NULL) return 0;
    size_t n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
    return 1;
}

void printClock(int mins) {
    printf("%02d:%02d", mins / 60, mins % 60);
}

/* ---------- Room helpers ---------- */

void showAllRooms(void) {
    int i;
    printf("\n--- CIT 4th Floor Rooms ---\n");
    printf("%-12s %-10s %-10s\n", "Room", "Capacity", "Status");
    for (i = 0; i < roomCount; i++) {
        printf("%-12s %-10d %-10s\n",
               citFloor[i].roomTag,
               citFloor[i].seatLimit,
               citFloor[i].isUsable ? "Usable" : "Closed");
    }
}

void listRoomsThatFit(int classSize) {
    int i, found = 0;
    printf("\nRooms that can hold %d students:\n", classSize);
    for (i = 0; i < roomCount; i++) {
        if (citFloor[i].isUsable && citFloor[i].seatLimit >= classSize) {
            printf("  %s (capacity %d)\n", citFloor[i].roomTag, citFloor[i].seatLimit);
            found = 1;
        }
    }
    if (!found) printf("  None. Class size is larger than every usable room.\n");
}

/* Best-fit = smallest capacity that still fits, and free at that day/time */
const char *suggestBestRoom(int classSize, const char *day, int startMins, int endMins) {
    int i, j, bestIdx = -1, bestCap = 99999;
    int dayKey = dayToKey(day);

    for (i = 0; i < roomCount; i++) {
        int freeAtTime = 1;
        if (!citFloor[i].isUsable) continue;
        if (citFloor[i].seatLimit < classSize) continue;

        for (j = 0; j < slotCount; j++) {
            if (dayToKey(timetable[j].weekDay) != dayKey) continue;
            if (strcmp(timetable[j].roomTag, citFloor[i].roomTag) != 0) continue;
            if (timesOverlap(startMins, endMins, timetable[j].startMins, timetable[j].endMins)) {
                freeAtTime = 0;
                break;
            }
        }
        if (!freeAtTime) continue;

        if (citFloor[i].seatLimit < bestCap) {
            bestCap = citFloor[i].seatLimit;
            bestIdx = i;
        }
    }
    return (bestIdx >= 0) ? citFloor[bestIdx].roomTag : NULL;
}

/* ---------- Conflict checks ---------- */

int checkTimeConflict(const ClassSlot *newbie, char *reason, int reasonLen) {
    int i, key = dayToKey(newbie->weekDay);
    for (i = 0; i < slotCount; i++) {
        if (dayToKey(timetable[i].weekDay) != key) continue;
        /* Same subject code same day overlapping = schedule clash for that section */
        if (strcmp(timetable[i].subjCode, newbie->subjCode) == 0 &&
            timesOverlap(newbie->startMins, newbie->endMins,
                         timetable[i].startMins, timetable[i].endMins)) {
            snprintf(reason, reasonLen,
                     "TIME CONFLICT: %s already set %s ",
                     timetable[i].subjCode, timetable[i].weekDay);
            /* append times */
            {
                char t1[16], t2[16];
                snprintf(t1, sizeof(t1), "%02d:%02d", timetable[i].startMins / 60, timetable[i].startMins % 60);
                snprintf(t2, sizeof(t2), "%02d:%02d", timetable[i].endMins / 60, timetable[i].endMins % 60);
                strncat(reason, t1, reasonLen - strlen(reason) - 1);
                strncat(reason, "-", reasonLen - strlen(reason) - 1);
                strncat(reason, t2, reasonLen - strlen(reason) - 1);
            }
            return 1;
        }
    }
    return 0;
}

int checkRoomConflict(const ClassSlot *newbie, char *reason, int reasonLen) {
    int i, key = dayToKey(newbie->weekDay);
    for (i = 0; i < slotCount; i++) {
        if (dayToKey(timetable[i].weekDay) != key) continue;
        if (strcmp(timetable[i].roomTag, newbie->roomTag) != 0) continue;
        if (timesOverlap(newbie->startMins, newbie->endMins,
                         timetable[i].startMins, timetable[i].endMins)) {
            snprintf(reason, reasonLen,
                     "ROOM CONFLICT: %s already used by %s (%s ",
                     newbie->roomTag, timetable[i].subjCode, timetable[i].weekDay);
            {
                char t1[16], t2[16];
                snprintf(t1, sizeof(t1), "%02d:%02d", timetable[i].startMins / 60, timetable[i].startMins % 60);
                snprintf(t2, sizeof(t2), "%02d:%02d", timetable[i].endMins / 60, timetable[i].endMins % 60);
                strncat(reason, t1, reasonLen - strlen(reason) - 1);
                strncat(reason, "-", reasonLen - strlen(reason) - 1);
                strncat(reason, t2, reasonLen - strlen(reason) - 1);
                strncat(reason, ")", reasonLen - strlen(reason) - 1);
            }
            return 1;
        }
    }
    return 0;
}

int checkCapacity(const ClassSlot *newbie, char *reason, int reasonLen) {
    FloorRoom *rm = findRoom(newbie->roomTag);
    if (rm == NULL) {
        snprintf(reason, reasonLen, "Room %s is not in the CIT 4th-floor list.", newbie->roomTag);
        return 1;
    }
    if (!rm->isUsable) {
        snprintf(reason, reasonLen, "Room %s is marked not usable.", newbie->roomTag);
        return 1;
    }
    if (newbie->headCount > rm->seatLimit) {
        snprintf(reason, reasonLen,
                 "CAPACITY CONFLICT: class size %d > room %s capacity %d.",
                 newbie->headCount, rm->roomTag, rm->seatLimit);
        return 1;
    }
    return 0;
}

void listSchedule(void) {
    int i;
    if (slotCount == 0) {
        printf("\nNo classes scheduled yet.\n");
        return;
    }
    printf("\n--- Current Schedule (CIT 4F) ---\n");
    printf("%-4s %-10s %-20s %-6s %-11s %-10s %-5s\n",
           "#", "Code", "Title", "Day", "Time", "Room", "Size");
    for (i = 0; i < slotCount; i++) {
        printf("%-4d %-10s %-20s %-6s ",
               i + 1,
               timetable[i].subjCode,
               timetable[i].subjTitle,
               timetable[i].weekDay);
        printClock(timetable[i].startMins);
        printf("-");
        printClock(timetable[i].endMins);
        printf("  %-10s %-5d\n",
               timetable[i].roomTag,
               timetable[i].headCount);
    }
}

void addSchedule(void) {
    ClassSlot draft;
    char clockBuf[32], reason[160];
    const char *hint;

    if (slotCount >= MAX_SLOTS) {
        printf("Schedule is full.\n");
        return;
    }

    memset(&draft, 0, sizeof(draft));

    printf("Subject code (e.g. CPET1L-M): ");
    readLineTrim(draft.subjCode, MAX_CODE_LEN);

    printf("Subject title: ");
    readLineTrim(draft.subjTitle, MAX_NAME_LEN);

    printf("Day (Mon/Tue/Wed/Thu/Fri/Sat): ");
    readLineTrim(draft.weekDay, MAX_DAY_LEN);
    if (dayToKey(draft.weekDay) == 0) {
        printf("Invalid day.\n");
        return;
    }

    printf("Start time (24-Hour Format, e.g. 13:30): ");
    readLineTrim(clockBuf, sizeof(clockBuf));
    if (!parseClock(clockBuf, &draft.startMins)) {
        printf("Invalid start time.\n");
        return;
    }

    printf("End time (24-Hour Format, e.g. 15:00): ");
    readLineTrim(clockBuf, sizeof(clockBuf));
    if (!parseClock(clockBuf, &draft.endMins)) {
        printf("Invalid end time.\n");
        return;
    }
    if (draft.endMins <= draft.startMins) {
        printf("End must be after start.\n");
        return;
    }

    printf("Class size (number of students): ");
    if (scanf("%d", &draft.headCount) != 1 || draft.headCount <= 0) {
        clearInputTrail();
        printf("Invalid class size.\n");
        return;
    }
    clearInputTrail();

    listRoomsThatFit(draft.headCount);
    hint = suggestBestRoom(draft.headCount, draft.weekDay, draft.startMins, draft.endMins);
    if (hint != NULL)
        printf("Suggested best-fit free room: %s\n", hint);
    else
        printf("No free room fits this size at that time.\n");

    printf("Room tag (e.g. CIT-401): ");
    readLineTrim(draft.roomTag, MAX_ROOM_LEN);

    if (checkCapacity(&draft, reason, sizeof(reason))) {
        printf("%s\n", reason);
        return;
    }
    if (checkRoomConflict(&draft, reason, sizeof(reason))) {
        printf("%s\n", reason);
        return;
    }
    if (checkTimeConflict(&draft, reason, sizeof(reason))) {
        printf("%s\n", reason);
        /* time conflict on same subject code is a hard block */
        return;
    }

    timetable[slotCount++] = draft;
    scheduleChanged = 1;
    printf("Saved: %s | %s ", draft.subjCode, draft.weekDay);
    printClock(draft.startMins);
    printf("-");
    printClock(draft.endMins);
    printf(" @ %s (size %d)\n", draft.roomTag, draft.headCount);
}

void deleteSchedule(void) {
    int pick;
    listSchedule();
    if (slotCount == 0) return;
    printf("Delete which #? ");
    if (scanf("%d", &pick) != 1) { clearInputTrail(); return; }
    clearInputTrail();
    if (pick < 1 || pick > slotCount) {
        printf("Out of range.\n");
        return;
    }
    {
        int i;
        for (i = pick - 1; i < slotCount - 1; i++)
            timetable[i] = timetable[i + 1];
        slotCount--;
    }
    scheduleChanged = 1;
    printf("Deleted.\n");
}

void scanAllConflicts(void) {
    int i, j, found = 0;
    printf("\n--- Conflict Scan ---\n");
    for (i = 0; i < slotCount; i++) {
        for (j = i + 1; j < slotCount; j++) {
            if (dayToKey(timetable[i].weekDay) != dayToKey(timetable[j].weekDay)) continue;
            if (!timesOverlap(timetable[i].startMins, timetable[i].endMins,
                              timetable[j].startMins, timetable[j].endMins)) continue;

            if (strcmp(timetable[i].roomTag, timetable[j].roomTag) == 0) {
                printf("ROOM: %s used by both %s and %s on %s\n",
                       timetable[i].roomTag,
                       timetable[i].subjCode, timetable[j].subjCode,
                       timetable[i].weekDay);
                found = 1;
            }
            if (strcmp(timetable[i].subjCode, timetable[j].subjCode) == 0) {
                printf("TIME: %s overlaps itself on %s\n",
                       timetable[i].subjCode, timetable[i].weekDay);
                found = 1;
            }
        }
        {
            FloorRoom *rm = findRoom(timetable[i].roomTag);
            if (rm && timetable[i].headCount > rm->seatLimit) {
                printf("CAPACITY: %s size %d > %s cap %d\n",
                       timetable[i].subjCode, timetable[i].headCount,
                       rm->roomTag, rm->seatLimit);
                found = 1;
            }
        }
    }
    if (!found) printf("No conflicts found.\n");
}

void editRoomCapacity(void) {
    char tag[MAX_ROOM_LEN];
    FloorRoom *rm;
    int newCap, usable;
    showAllRooms();
    printf("Room tag to edit: ");
    readLineTrim(tag, MAX_ROOM_LEN);
    rm = findRoom(tag);
    if (!rm) { printf("Not found.\n"); return; }
    printf("New capacity: ");
    if (scanf("%d", &newCap) != 1 || newCap <= 0) { clearInputTrail(); return; }
    printf("Usable? (1=yes 0=no): ");
    if (scanf("%d", &usable) != 1) { clearInputTrail(); return; }
    clearInputTrail();
    rm->seatLimit = newCap;
    rm->isUsable  = usable ? 1 : 0;
    printf("Updated %s.\n", rm->roomTag);
    roomsChanged = 1;
    saveRooms();         /* save to rooms.txt right away, so the Room Capacity Module sees it too */
}

/* ---------- Menu ---------- */

/* Called when the person leaves the schedule menu: do not lose changes by accident */
static void leaveScheduleMenu(int inputEnded) {
    int answer = -1, result;

    if (!scheduleChanged) {
        return;
    }
    if (inputEnded) {
        printf("\nInput closed. Saving your changes before exit.\n");
        if (saveSchedule()) scheduleChanged = 0;
        return;
    }

    while (answer != 0 && answer != 1) {
        printf("You have unsaved changes. Save now? (1 = yes, 0 = no): ");
        result = scanf("%d", &answer);
        if (result == EOF) {
            printf("\nInput closed. Saving your changes before exit.\n");
            if (saveSchedule()) scheduleChanged = 0;
            return;
        }
        clearInputTrail();
        if (result != 1) {
            answer = -1;
        }
    }
    if (answer == 1 && saveSchedule()) {
        scheduleChanged = 0;
    }
}

void scheduleMenu(void) {
    int choice, result;
    printf("\nTip: when adding a class, enter class size first -\n");
    printf("     the program lists fitting rooms and suggests the best free one.\n");
    for (;;) {
        printf("\n========================================\n");
        printf("  SCHEDULE (CIT 4th Floor only)\n");
        printf("========================================\n");
        printf("1. Add class schedule\n");
        printf("2. List schedules\n");
        printf("3. Delete schedule\n");
        printf("4. Scan for conflicts\n");
        printf("5. Show CIT 4F rooms\n");
        printf("6. List rooms that fit a class size\n");
        printf("7. Edit room capacity / usability\n");
        printf("8. Save schedule\n");
        printf("0. Exit\n");
        printf("Choice: ");
        result = scanf("%d", &choice);
        if (result == EOF) {
            leaveScheduleMenu(1);   /* input ended: save, then go back instead of looping forever */
            return;
        }
        if (result != 1) {
            clearInputTrail();
            continue;
        }
        clearInputTrail();

        switch (choice) {
            case 1: addSchedule(); break;
            case 2: listSchedule(); break;
            case 3: deleteSchedule(); break;
            case 4: scanAllConflicts(); break;
            case 5: showAllRooms(); break;
            case 6: {
                int sz;
                printf("Class size: ");
                if (scanf("%d", &sz) == 1) listRoomsThatFit(sz);
                clearInputTrail();
                break;
            }
            case 7: editRoomCapacity(); break;
            case 8:
                if (saveSchedule()) scheduleChanged = 0;
                break;
            case 0:
                leaveScheduleMenu(0);
                return;
            default: printf("Invalid choice.\n");
        }
    }
}