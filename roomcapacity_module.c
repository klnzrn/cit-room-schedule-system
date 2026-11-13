/* Room Capacity Module

   Helps users find which rooms are free for a given day, time and
   number of attendees. Rooms can be added, edited, deleted and saved
   to a file (rooms.txt).

   isUsable: 1 = usable, 0 = closed (storage, faculty room, or not in use).

*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "common.h"

/* Shared data and helper functions (citFloor, roomCount, timetable, slotCount,
   clearInputTrail, dayToKey, parseClock, timesOverlap, findRoom, loadRooms, saveRooms, roomsChanged) are in
   common.h / common.c. This file has no main(): main.c starts it with roomMenu(). */

/* reads a whole number, returns -1 if the user typed something else
   (and 0 if the input has ended, so the menu closes instead of looping) */
int readNumber() {
    int n, result;

    result = scanf("%d", &n);
    if (result == EOF) {
        return 0;
    }
    clearInputTrail();
    if (result != 1) {
        return -1;
    }
    return n;
}

/* returns 1 if any scheduled class uses this room, otherwise 0 */
int roomHasClasses(const char *tag) {
    int j;

    for (j = 0; j < slotCount; j++) {
        if (strcmp(timetable[j].roomTag, tag) == 0) {
            return 1;
        }
    }
    return 0;
}

/* shows every room with its number */
void listRooms() {
    int i;

    if (roomCount == 0) {
        printf("No rooms yet.\n");
        return;
    }
    printf("\n%-4s %-10s %-10s %-9s %s\n", "No.", "Room", "Type", "Capacity", "Status");
    for (i = 0; i < roomCount; i++) {
        printf("%-4d %-10s %-10s %-9d ", i + 1, citFloor[i].roomTag,
               citFloor[i].roomType, citFloor[i].seatLimit);
        if (citFloor[i].isUsable == 1) {
            printf("Usable\n");
        } else {
            printf("Closed\n");
        }
    }
}

void addRoom() {
    int capacity, usable;

    if (roomCount >= MAX_ROOMS) {
        printf("Room list is full.\n");
        return;
    }

    printf("Enter room tag (e.g. CIT-412): ");
    scanf("%23s", citFloor[roomCount].roomTag);
    if (findRoom(citFloor[roomCount].roomTag) != NULL) {
        printf("That room already exists.\n");
        return;
    }

    printf("Enter room type (lab, lec, faculty, storage): ");
    scanf("%15s", citFloor[roomCount].roomType);

    printf("Enter capacity (0 if none): ");
    capacity = readNumber();
    if (capacity < 0) {
        printf("Invalid capacity.\n");
        return;
    }

    printf("Usable for scheduling? (1 = yes, 0 = no): ");
    usable = readNumber();
    if (usable != 0 && usable != 1) {
        printf("Invalid answer.\n");
        return;
    }

    citFloor[roomCount].seatLimit = capacity;
    citFloor[roomCount].isUsable = usable;
    roomCount++;
    roomsChanged = 1;
    printf("Room added.\n");
}

void editRoom() {
    int num, choice, value;
    char newTag[MAX_ROOM_LEN];

    if (roomCount == 0) {
        printf("No rooms yet.\n");
        return;
    }

    listRooms();
    printf("Room number to edit: ");
    num = readNumber();
    if (num < 1 || num > roomCount) {
        printf("Invalid room number.\n");
        return;
    }

    printf("1. Room tag\n");
    printf("2. Room type\n");
    printf("3. Capacity\n");
    printf("4. Usable / closed\n");
    printf("What do you want to change? ");
    choice = readNumber();

    if (choice == 1) {
        if (roomHasClasses(citFloor[num - 1].roomTag) == 1) {
            printf("This room still has scheduled classes, so it cannot be renamed.\n");
            return;
        }
        printf("New room tag: ");
        scanf("%23s", newTag);
        if (findRoom(newTag) != NULL) {
            printf("That room tag is already used.\n");
            return;
        }
        strcpy(citFloor[num - 1].roomTag, newTag);
    } else if (choice == 2) {
        printf("New room type: ");
        scanf("%15s", citFloor[num - 1].roomType);
    } else if (choice == 3) {
        printf("New capacity: ");
        value = readNumber();
        if (value < 0) {
            printf("Invalid capacity.\n");
            return;
        }
        citFloor[num - 1].seatLimit = value;
    } else if (choice == 4) {
        printf("Usable? (1 = yes, 0 = no): ");
        value = readNumber();
        if (value != 0 && value != 1) {
            printf("Invalid answer.\n");
            return;
        }
        citFloor[num - 1].isUsable = value;
    } else {
        printf("Invalid choice.\n");
        return;
    }

    roomsChanged = 1;
    printf("Room updated.\n");
}

void deleteRoom() {
    int num, i;

    if (roomCount == 0) {
        printf("No rooms yet.\n");
        return;
    }

    listRooms();
    printf("Room number to delete: ");
    num = readNumber();
    if (num < 1 || num > roomCount) {
        printf("Invalid room number.\n");
        return;
    }
    if (roomHasClasses(citFloor[num - 1].roomTag) == 1) {
        printf("This room still has scheduled classes, so it cannot be deleted.\n");
        return;
    }

    /* move every room after it one place up */
    for (i = num - 1; i < roomCount - 1; i++) {
        citFloor[i] = citFloor[i + 1];
    }
    roomCount--;
    roomsChanged = 1;
    printf("Room deleted.\n");
}

/* returns 1 if a scheduled class already uses this room at that day and time */
int isRoomBooked(const char *tag, int dayKey, int startMins, int endMins) {
    int j;

    for (j = 0; j < slotCount; j++) {
        if (strcmp(timetable[j].roomTag, tag) == 0 &&
            dayToKey(timetable[j].weekDay) == dayKey &&
            timesOverlap(startMins, endMins, timetable[j].startMins, timetable[j].endMins)) {
            return 1;
        }
    }
    return 0;
}

/* asks for the day, time and attendees, then lists the rooms that qualify */
void findAvailableRooms() {
    char day[MAX_DAY_LEN] = "", clockBuf[32] = "";
    int dayKey, startMins, endMins, people;
    int found[MAX_ROOMS];              /* positions of the rooms that qualify */
    int count = 0, bigEnough = 0;
    int i, j, temp;

    printf("Day (Mon/Tue/Wed/Thu/Fri/Sat): ");
    scanf("%11s", day);
    dayKey = dayToKey(day);
    if (dayKey == 0) {
        printf("Invalid day.\n");
        return;
    }

    printf("Start time (24-Hour Format, e.g. 13:30): ");
    scanf("%31s", clockBuf);
    if (parseClock(clockBuf, &startMins) == 0) {
        printf("Invalid start time.\n");
        return;
    }

    printf("End time (24-Hour Format, e.g. 15:00): ");
    scanf("%31s", clockBuf);
    if (parseClock(clockBuf, &endMins) == 0) {
        printf("Invalid end time.\n");
        return;
    }
    if (endMins <= startMins) {
        printf("End must be after start.\n");
        return;
    }

    printf("Number of attendees: ");
    people = readNumber();
    if (people < 1) {
        printf("Invalid number of attendees.\n");
        return;
    }

    /* keep the rooms that are usable, big enough and not booked */
    for (i = 0; i < roomCount; i++) {
        if (citFloor[i].isUsable == 1 && citFloor[i].seatLimit >= people) {
            bigEnough++;
            if (isRoomBooked(citFloor[i].roomTag, dayKey, startMins, endMins) == 0) {
                found[count] = i;
                count++;
            }
        }
    }

    if (count == 0) {
        if (bigEnough == 0) {
            printf("\nNo room qualifies: no usable room can hold %d attendees.\n", people);
        } else {
            printf("\nNo room qualifies: every room that can hold %d attendees is already booked at that time.\n", people);
        }
        return;
    }

    /* sort the rooms by capacity, smallest first (bubble sort) */
    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - 1 - i; j++) {
            if (citFloor[found[j]].seatLimit > citFloor[found[j + 1]].seatLimit) {
                temp = found[j];
                found[j] = found[j + 1];
                found[j + 1] = temp;
            }
        }
    }

    printf("\nFree on %s %02d:%02d-%02d:%02d for %d attendee(s), smallest capacity first:\n",
           day, startMins / 60, startMins % 60, endMins / 60, endMins % 60, people);
    printf("%-4s %-10s %-10s %s\n", "No.", "Room", "Type", "Capacity");
    for (i = 0; i < count; i++) {
        printf("%-4d %-10s %-10s %d\n", i + 1, citFloor[found[i]].roomTag,
               citFloor[found[i]].roomType, citFloor[found[i]].seatLimit);
    }
    printf("\nBest fit: %s (capacity %d)\n", citFloor[found[0]].roomTag,
           citFloor[found[0]].seatLimit);
}

/* TESTING ONLY: puts one made-up class in the timetable */
void addSampleClass(const char *code, const char *title, const char *day,
                    int startMins, int endMins, const char *tag, int size) {
    strcpy(timetable[slotCount].subjCode, code);
    strcpy(timetable[slotCount].subjTitle, title);
    strcpy(timetable[slotCount].weekDay, day);
    timetable[slotCount].startMins = startMins;
    timetable[slotCount].endMins = endMins;
    strcpy(timetable[slotCount].roomTag, tag);
    timetable[slotCount].headCount = size;
    slotCount++;
}

void loadSampleBookings() {
    slotCount = 0;
    addSampleClass("BET-CpET 1F", "Intro to Computer Programming", "Mon", 7 * 60 + 30, 9 * 60, "CIT-402", 25);
    addSampleClass("BET-CpET 1D", "Programming 1", "Mon", 9 * 60, 10 * 60 + 30, "CIT-403", 25);
    addSampleClass("BET-CpET 2A", "Data Structures", "Wed", 13 * 60, 15 * 60, "CIT-423", 40);
    printf("Loaded %d sample classes:\n", slotCount);
    printf("  BET-CpET 1F Mon 07:30-09:00 CIT-402\n");
    printf("  BET-CpET 1D Mon 09:00-10:30 CIT-403\n");
    printf("  BET-CpET 2A Wed 13:00-15:00 CIT-423\n");
}

void roomMenu() {
    int choice, answer;

    do {
        printf("\n--- ROOM CAPACITY MENU ---\n");
        printf("1. Add room\n");
        printf("2. Edit room\n");
        printf("3. Delete room\n");
        printf("4. Show all rooms\n");
        printf("5. Find available rooms\n");
        printf("6. Save\n");
        printf("0. Exit\n");
        printf("Choice: ");
        choice = readNumber();

        if (choice == 1) addRoom();
        else if (choice == 2) editRoom();
        else if (choice == 3) deleteRoom();
        else if (choice == 4) listRooms();
        else if (choice == 5) findAvailableRooms();
        else if (choice == 6) saveRooms();
        else if (choice != 0) printf("Invalid choice.\n");
    } while (choice != 0);

    /* do not lose changes by accident */
    if (roomsChanged == 1) {
        answer = -1;
        while (answer != 0 && answer != 1) {
            printf("You have unsaved changes. Save now? (1 = yes, 0 = no): ");
            answer = readNumber();
        }
        if (answer == 1) {
            saveRooms();
        }
    }
}