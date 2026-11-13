/* mainmenu.c - Main Menu
 *
 * Integrated Student Records and Class Scheduling System (Group: Java Rice)
 * Scope: CIT Department
 *
 * This is the ONLY file with a main(). It connects the modules:
 *   1. Attendance      (attendance.c)       -> attendanceMenu()
 *   2. Class Schedule  (schedule_module.c)  -> scheduleMenu()
 *   3. Room Capacity   (roomcapacity.c)     -> roomMenu()
 * common.h / common.c hold what the modules share (the room list, the class
 * timetable, and the helper functions), so a class added in the Schedule
 * module is seen by the Room Capacity module straight away.
 *
 * To run the program:
 * 1. Type "cl /nologo /Fe:mainmenu.exe mainmenu.c attendance_module.c schedule_module.c roomcapacity_module.c common.c"
 * 2. Then, type "mainmenu.exe" to run the program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

/* the menus that live in the other files */
void attendanceMenu(void);   /* attendance.c */
void scheduleMenu(void);     /* schedule_module.c */
void roomMenu(void);         /* roomcapacity.c */

/* Reads a menu choice from min to max.
   Returns 1 = OK, 0 = invalid (message shown), -1 = input ended (Ctrl+D / Ctrl+Z) */
static int readChoice(int min, int max, int *out) {
    char line[64];
    char *end;
    long value;

    printf("Choice: ");
    if (fgets(line, sizeof line, stdin) == NULL) {
        return -1;
    }

    value = strtol(line, &end, 10);
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    if (end == line || (*end != '\n' && *end != '\0') || value < min || value > max) {
        printf("Please enter a number from %d to %d.\n", min, max);
        return 0;
    }

    *out = (int)value;
    return 1;
}

static void printMainMenu(void) {
    printf("\n========================================================\n");
    printf("  INTEGRATED STUDENT RECORDS AND CLASS SCHEDULING SYSTEM\n");
    printf("  CIT Department  |  Group: Java Rice\n");
    printf("========================================================\n");
    printf("  Rooms: %d   |   Scheduled classes: %d\n", roomCount, slotCount);
    printf("--------------------------------------------------------\n");
    printf("  1. Attendance\n");
    printf("  2. Class Schedule\n");
    printf("  3. Room Capacity\n");
    printf("  0. Exit\n");
    printf("--------------------------------------------------------\n");
}

int main(void) {
    int choice;
    int running = 1;

    loadRooms();     /* one shared room list for the Schedule and Room Capacity modules */
    loadSchedule();  /* one shared timetable, loaded from schedule.txt */

    while (running) {
        int result;

        printMainMenu();
        result = readChoice(0, 3, &choice);
        if (result == -1) {
            break;               /* input ended: leave the program */
        }
        if (result == 0) {
            continue;            /* invalid input: show the menu again */
        }

        switch (choice) {
            case 1: attendanceMenu(); break;
            case 2: scheduleMenu();   break;
            case 3: roomMenu();       break;
            case 0: running = 0;      break;
        }
    }

    printf("\nThank you for using the JAVA RICE system. Goodbye!\n");
    return 0;
}
