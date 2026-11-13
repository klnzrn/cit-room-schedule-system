/* Attendance Module (polished version)
 *
 * P = present, A = absent, L = late, '-' = not recorded yet
 * Late counts as half (0.5) of a present.
 *
 * Features:
 *   - The class to record is picked from the Class Schedule module
 *     (the subject code is used as the section; each class has its own file)
 *   - Reports show the subject, days, times and room taken from the schedule,
 *     and compare the students listed with the class size on the schedule
 *   - Add, edit, delete, and search students
 *   - Mark attendance (warns before overwriting a mark)
 *   - Month and year support (days are checked against the real month length)
 *   - Per-student and whole-class reports with P / A / L counts
 *   - Safe input (no infinite loops, no buffer overflows)
 *   - Safe file loading (a damaged save file cannot crash the program)
 *   - "Save before exit?" prompt when there are unsaved changes
 *
 * This file has no main(). It is started from main.c by calling attendanceMenu().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "common.h"      /* shared timetable (slotCount / timetable) */

#define MAX_STUDENTS 30
#define MAX_DAYS     31
#define NAME_SIZE    50              /* 49 characters + the '\0' at the end */
#define SECTION_SIZE 30              /* 29 characters + the '\0' at the end */
#define SAVE_PREFIX  "attendance_"   /* file name: attendance_<section>.txt */
#define SAVE_SUFFIX  ".txt"
#define FILE_HEADER  "ATTENDANCE_V3"

/* ---------- Data structures ---------- */

/* One "index card" per student: name and daily records together */
typedef struct {
    char name[NAME_SIZE];
    char days[MAX_DAYS];             /* days[0] is day 1, and so on */
} Student;

/* Everything about the class lives here, so we pass one pointer around
   instead of using global variables. */
typedef struct {
    char section[SECTION_SIZE];      /* for example "Section A" or "10-Newton" */
    Student students[MAX_STUDENTS];
    int total;                       /* number of students */
    int month;                       /* 1-12 */
    int year;
    int dirty;                       /* 1 = there are unsaved changes */
} ClassData;

static const char *MONTH_NAMES[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

/* Set to 1 when the input stream ends (Ctrl+D / Ctrl+Z) */
static int inputClosed = 0;

/* ---------- Small helpers ---------- */

static int isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int daysInMonth(int month, int year) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return days[month - 1];
}

/* Remove spaces at the start and end of a string */
static void trim(char *s) {
    char *start = s;
    size_t len;

    while (isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static int equalsIgnoreCase(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int containsIgnoreCase(const char *text, const char *part) {
    size_t i, j;
    size_t n = strlen(text);
    size_t m = strlen(part);

    if (m == 0) {
        return 1;
    }
    for (i = 0; i + m <= n; i++) {
        for (j = 0; j < m; j++) {
            if (tolower((unsigned char)text[i + j]) !=
                tolower((unsigned char)part[j])) {
                break;
            }
        }
        if (j == m) {
            return 1;
        }
    }
    return 0;
}

/* ---------- Safe input ---------- */

/* Reads one line, removes the newline, and throws away anything that did not
   fit in the buffer. Returns 0 if the input has ended. */
static int readLine(char *buf, int size) {
    size_t len;

    if (fgets(buf, size, stdin) == NULL) {
        return 0;
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else {
        int c;                       /* line was too long: discard the rest */
        while ((c = getchar()) != '\n' && c != EOF) {
            ;
        }
    }
    return 1;
}

/* Asks for a whole number between min and max.
   Returns 1 on success, 0 if the input was invalid. */
static int readIntInRange(const char *prompt, int min, int max, int *out) {
    char line[64];
    char *end;
    long value;

    printf("%s", prompt);
    if (!readLine(line, sizeof line)) {
        inputClosed = 1;
        return 0;
    }

    value = strtol(line, &end, 10);
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    if (end == line || *end != '\0' || value < min || value > max) {
        printf("Please enter a number from %d to %d.\n", min, max);
        return 0;
    }

    *out = (int)value;
    return 1;
}

/* Asks a yes/no question. Returns 1 for yes, 0 for no (or end of input). */
static int readYesNo(const char *prompt) {
    char line[32];

    for (;;) {
        char answer;

        printf("%s", prompt);
        if (!readLine(line, sizeof line)) {
            inputClosed = 1;
            return 0;
        }
        trim(line);
        answer = (char)tolower((unsigned char)line[0]);
        if (answer == 'y') {
            return 1;
        }
        if (answer == 'n') {
            return 0;
        }
        printf("Please answer y or n.\n");
    }
}

/* Asks for a student name. Returns 1 on success. */
static int readName(const char *prompt, char out[NAME_SIZE]) {
    char buf[128];

    printf("%s", prompt);
    if (!readLine(buf, sizeof buf)) {
        inputClosed = 1;
        return 0;
    }
    trim(buf);

    if (buf[0] == '\0') {
        printf("Name cannot be empty.\n");
        return 0;
    }
    if (strlen(buf) >= NAME_SIZE) {
        printf("Name is too long (maximum %d characters).\n", NAME_SIZE - 1);
        return 0;
    }

    strcpy(out, buf);
    return 1;
}

/* Builds the save file name from the section name.
   "Section A" -> "attendance_section_a.txt"
   Letters and numbers are kept (lowercase), spaces, '-' and '_' become '_',
   and any other symbol is dropped. This also means "section a" and
   "SECTION A" use the same file. */
static void buildFileName(const ClassData *c, char *out, size_t size) {
    char part[SECTION_SIZE];
    size_t i, n = 0;

    for (i = 0; c->section[i] != '\0' && n < sizeof part - 1; i++) {
        unsigned char ch = (unsigned char)c->section[i];
        if (isalnum(ch)) {
            part[n++] = (char)tolower(ch);
        } else if (ch == ' ' || ch == '-' || ch == '_') {
            part[n++] = '_';
        }
    }
    part[n] = '\0';

    snprintf(out, size, "%s%s%s", SAVE_PREFIX, part, SAVE_SUFFIX);
}

/* ---------- Linking to the Class Schedule module ---------- */

/* the first scheduled meeting of this class (subject code), or NULL if it is not on the schedule */
static const ClassSlot *firstMeeting(const char *code) {
    int i;
    for (i = 0; i < slotCount; i++) {
        if (strcmp(timetable[i].subjCode, code) == 0) {
            return &timetable[i];
        }
    }
    return NULL;
}

/* prints every meeting of this class, for example:
   Mon 07:30-09:00 @ CIT-402; Wed 13:00-14:30 @ CIT-423 */
static void printMeetings(const char *code) {
    int i, shown = 0;
    for (i = 0; i < slotCount; i++) {
        if (strcmp(timetable[i].subjCode, code) == 0) {
            if (shown > 0) {
                printf("; ");
            }
            printf("%s %02d:%02d-%02d:%02d @ %s", timetable[i].weekDay,
                   timetable[i].startMins / 60, timetable[i].startMins % 60,
                   timetable[i].endMins / 60, timetable[i].endMins % 60,
                   timetable[i].roomTag);
            shown++;
        }
    }
}

/* the largest class size on the schedule for this class, or 0 if it is not scheduled */
static int scheduledClassSize(const char *code) {
    int i, size = 0;
    for (i = 0; i < slotCount; i++) {
        if (strcmp(timetable[i].subjCode, code) == 0 && timetable[i].headCount > size) {
            size = timetable[i].headCount;
        }
    }
    return size;
}

/* shows the subject and meeting times of the class, taken from the schedule */
static void printClassInfo(const char *code) {
    const ClassSlot *first = firstMeeting(code);

    if (first == NULL) {
        printf("(This class is no longer on the schedule.)\n");
        return;
    }
    printf("Subject: %s\n", first->subjTitle);
    printf("Schedule: ");
    printMeetings(code);
    printf("\n");
}

/* Lets the person pick one of the scheduled classes.
   Returns 1 and fills out[] with the subject code, or 0 to go back. */
static int chooseClass(char out[SECTION_SIZE]) {
    int firstIdx[MAX_SLOTS];         /* position of the first meeting of each class */
    int count = 0, i, j, pick;

    /* each subject code is listed once, even if it meets on several days */
    for (i = 0; i < slotCount; i++) {
        int seen = 0;

        if (timetable[i].subjCode[0] == '\0') {
            continue;                /* a class without a subject code cannot be recorded */
        }
        for (j = 0; j < i; j++) {
            if (strcmp(timetable[j].subjCode, timetable[i].subjCode) == 0) {
                seen = 1;
                break;
            }
        }
        if (!seen) {
            firstIdx[count++] = i;
        }
    }

    if (count == 0) {
        printf("\nNo classes are scheduled yet. Add a class in Class Schedule first.\n");
        return 0;
    }

    printf("\nWhich class are you recording?\n");
    for (i = 0; i < count; i++) {
        const ClassSlot *s = &timetable[firstIdx[i]];

        printf("%d. %s  %s  (", i + 1, s->subjCode, s->subjTitle);
        printMeetings(s->subjCode);
        printf(")\n");
    }

    for (;;) {
        if (readIntInRange("Class number (0 = back): ", 0, count, &pick)) {
            break;
        }
        if (inputClosed) {
            return 0;
        }
    }
    if (pick == 0) {
        return 0;
    }

    snprintf(out, SECTION_SIZE, "%s", timetable[firstIdx[pick - 1]].subjCode);
    return 1;
}

/* Asks for P / A / L. Returns the uppercase letter, or 0 if invalid. */
static char readStatus(void) {
    char line[32];
    char status;

    printf("Status (P = present, A = absent, L = late): ");
    if (!readLine(line, sizeof line)) {
        inputClosed = 1;
        return 0;
    }
    trim(line);

    status = (char)toupper((unsigned char)line[0]);
    if (line[1] != '\0' || (status != 'P' && status != 'A' && status != 'L')) {
        printf("Invalid status. Use P, A, or L.\n");
        return 0;
    }
    return status;
}

/* ---------- Student helpers ---------- */

static void clearRecords(Student *s) {
    memset(s->days, '-', MAX_DAYS);
}

static int hasAnyRecords(const ClassData *c) {
    int i, d;
    for (i = 0; i < c->total; i++) {
        for (d = 0; d < MAX_DAYS; d++) {
            if (c->students[i].days[d] != '-') {
                return 1;
            }
        }
    }
    return 0;
}

/* Returns the index of the student with this name, or -1 */
static int findByName(const ClassData *c, const char *name) {
    int i;
    for (i = 0; i < c->total; i++) {
        if (equalsIgnoreCase(c->students[i].name, name)) {
            return i;
        }
    }
    return -1;
}

static int countStatus(const Student *s, char status) {
    int d, count = 0;
    for (d = 0; d < MAX_DAYS; d++) {
        if (s->days[d] == status) {
            count++;
        }
    }
    return count;
}

/* Present = 1 point, Late = 0.5 point, Absent = 0 points.
   Returns 1 and fills *percent if the student has records, otherwise 0. */
static int computePercentage(const Student *s, float *percent) {
    int present = countStatus(s, 'P');
    int late    = countStatus(s, 'L');
    int absent  = countStatus(s, 'A');
    int counted = present + late + absent;

    if (counted == 0) {
        return 0;
    }
    *percent = (present + 0.5f * late) / counted * 100.0f;
    return 1;
}

static void listStudents(const ClassData *c) {
    int i;
    for (i = 0; i < c->total; i++) {
        printf("%d. %s\n", i + 1, c->students[i].name);
    }
}

/* Shows the list and asks which student. Returns the index or -1. */
static int pickStudent(const ClassData *c) {
    int num;

    if (c->total == 0) {
        printf("No students yet.\n");
        return -1;
    }
    listStudents(c);
    if (!readIntInRange("Student number: ", 1, c->total, &num)) {
        return -1;
    }
    return num - 1;
}

/* ---------- Menu actions ---------- */

static void addStudent(ClassData *c) {
    char name[NAME_SIZE];

    if (c->total >= MAX_STUDENTS) {
        printf("Class is full (maximum %d students).\n", MAX_STUDENTS);
        return;
    }
    if (!readName("Enter student name: ", name)) {
        return;
    }
    if (findByName(c, name) >= 0) {
        printf("A student with that name already exists.\n");
        return;
    }

    strcpy(c->students[c->total].name, name);
    clearRecords(&c->students[c->total]);
    c->total++;
    c->dirty = 1;
    printf("Student added.\n");
}

static void editStudent(ClassData *c) {
    char name[NAME_SIZE];
    int idx = pickStudent(c);
    int existing;

    if (idx < 0) {
        return;
    }
    if (!readName("New name: ", name)) {
        return;
    }
    existing = findByName(c, name);
    if (existing >= 0 && existing != idx) {
        printf("Another student already has that name.\n");
        return;
    }

    strcpy(c->students[idx].name, name);
    c->dirty = 1;
    printf("Name updated.\n");
}

static void deleteStudent(ClassData *c) {
    char prompt[NAME_SIZE + 64];
    int idx = pickStudent(c);
    int i;

    if (idx < 0) {
        return;
    }

    sprintf(prompt, "Delete %s and all their records? (y/n): ",
            c->students[idx].name);
    if (!readYesNo(prompt)) {
        printf("Cancelled.\n");
        return;
    }

    for (i = idx; i < c->total - 1; i++) {
        c->students[i] = c->students[i + 1];   /* shift everyone up */
    }
    c->total--;
    c->dirty = 1;
    printf("Student deleted.\n");
}

static void markAttendance(ClassData *c) {
    int idx, day, maxDay;
    char status, old;
    char prompt[64];

    idx = pickStudent(c);
    if (idx < 0) {
        return;
    }

    maxDay = daysInMonth(c->month, c->year);
    sprintf(prompt, "Day of the month (1-%d): ", maxDay);
    if (!readIntInRange(prompt, 1, maxDay, &day)) {
        return;
    }

    old = c->students[idx].days[day - 1];
    if (old != '-') {
        sprintf(prompt, "Day %d is already marked '%c'. Overwrite? (y/n): ",
                day, old);
        if (!readYesNo(prompt)) {
            printf("Cancelled.\n");
            return;
        }
    }

    status = readStatus();
    if (status == 0) {
        return;
    }

    c->students[idx].days[day - 1] = status;
    c->dirty = 1;
    printf("Attendance recorded (remember to Save).\n");
}

static void showStudent(const ClassData *c) {
    int idx = pickStudent(c);
    int d;
    float percent;
    const Student *s;

    if (idx < 0) {
        return;
    }
    s = &c->students[idx];

    printf("\nName: %s\n", s->name);
    printf("Class: %s\n", c->section);
    printClassInfo(c->section);
    printf("Month: %s %d\n", MONTH_NAMES[c->month - 1], c->year);
    for (d = 0; d < daysInMonth(c->month, c->year); d++) {
        if (s->days[d] != '-') {
            printf("Day %d: %c\n", d + 1, s->days[d]);
        }
    }

    printf("Present: %d   Late: %d   Absent: %d\n",
           countStatus(s, 'P'), countStatus(s, 'L'), countStatus(s, 'A'));

    if (!computePercentage(s, &percent)) {
        printf("No attendance records yet.\n");
    } else if (percent == 0.0f) {
        printf("Attendance: 0%% (never present)\n");
    } else {
        printf("Attendance: %.2f%%\n", percent);
    }
}

static void showClass(const ClassData *c) {
    int i, counted = 0, size;
    float percent, sum = 0;

    if (c->total == 0) {
        printf("No students yet.\n");
        return;
    }

    printf("\nClass report: %s - %s %d\n", c->section,
           MONTH_NAMES[c->month - 1], c->year);
    printClassInfo(c->section);
    printf("%-25s %4s %4s %4s  %s\n", "Name", "P", "L", "A", "Attendance");

    for (i = 0; i < c->total; i++) {
        const Student *s = &c->students[i];

        printf("%-25s %4d %4d %4d  ", s->name,
               countStatus(s, 'P'), countStatus(s, 'L'), countStatus(s, 'A'));

        if (computePercentage(s, &percent)) {
            printf("%.2f%%\n", percent);
            sum += percent;
            counted++;
        } else {
            printf("No records\n");
        }
    }

    if (counted == 0) {
        printf("Class average: none (no records yet)\n");
    } else {
        printf("Class average: %.2f%%\n", sum / counted);
    }

    size = scheduledClassSize(c->section);
    if (size > 0) {
        printf("Students listed: %d  |  Class size on schedule: %d\n", c->total, size);
        if (c->total > size) {
            printf("Warning: more students are listed than the class size on the schedule.\n");
        }
    }
}

static void searchStudents(const ClassData *c) {
    char query[NAME_SIZE];
    int i, found = 0;
    float percent;

    if (c->total == 0) {
        printf("No students yet.\n");
        return;
    }
    if (!readName("Search name: ", query)) {
        return;
    }

    for (i = 0; i < c->total; i++) {
        if (containsIgnoreCase(c->students[i].name, query)) {
            printf("%d. %s", i + 1, c->students[i].name);
            if (computePercentage(&c->students[i], &percent)) {
                printf("  (%.2f%%)", percent);
            }
            printf("\n");
            found++;
        }
    }
    if (found == 0) {
        printf("No student matches \"%s\".\n", query);
    }
}

static void changeMonth(ClassData *c) {
    int month, year, i;

    if (!readIntInRange("Month (1-12): ", 1, 12, &month)) {
        return;
    }
    if (!readIntInRange("Year (for example 2026): ", 1900, 9999, &year)) {
        return;
    }
    if (month == c->month && year == c->year) {
        printf("That month is already selected.\n");
        return;
    }
    if (hasAnyRecords(c) &&
        !readYesNo("This clears all attendance marks (students stay). "
                   "Continue? (y/n): ")) {
        printf("Cancelled.\n");
        return;
    }

    for (i = 0; i < c->total; i++) {
        clearRecords(&c->students[i]);
    }
    c->month = month;
    c->year = year;
    c->dirty = 1;
    printf("Now recording %s %d.\n", MONTH_NAMES[month - 1], year);
}

/* ---------- Saving and loading ---------- */

/* Returns 1 on success, 0 on failure */
static int saveFile(ClassData *c) {
    char fileName[80];
    FILE *fp;
    int i;

    buildFileName(c, fileName, sizeof fileName);
    fp = fopen(fileName, "w");
    if (fp == NULL) {
        printf("Cannot save file.\n");
        return 0;
    }

    fprintf(fp, "%s\n%s\n%d %d\n%d\n", FILE_HEADER, c->section,
            c->month, c->year, c->total);
    for (i = 0; i < c->total; i++) {
        fprintf(fp, "%s\n", c->students[i].name);
        fwrite(c->students[i].days, 1, MAX_DAYS, fp);
        fputc('\n', fp);
    }

    if (ferror(fp) || fclose(fp) != 0) {
        printf("Error while writing the file.\n");
        return 0;
    }

    c->dirty = 0;
    printf("Saved to %s\n", fileName);
    return 1;
}

/* Reads and checks the file. Returns 1 only if EVERYTHING is valid. */
static int parseFile(FILE *fp, ClassData *out) {
    char line[128];
    int i, d;

    /* Header */
    if (fgets(line, sizeof line, fp) == NULL) {
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, FILE_HEADER) != 0) {
        return 0;
    }

    /* Section name */
    if (fgets(line, sizeof line, fp) == NULL) {
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0' || strlen(line) >= SECTION_SIZE) {
        return 0;
    }
    strcpy(out->section, line);

    /* Month, year, number of students */
    if (fscanf(fp, "%d %d %d", &out->month, &out->year, &out->total) != 3) {
        return 0;
    }
    if (out->month < 1 || out->month > 12 ||
        out->year < 1900 || out->year > 9999 ||
        out->total < 0 || out->total > MAX_STUDENTS) {
        return 0;
    }
    if (fgets(line, sizeof line, fp) == NULL) {    /* rest of that line */
        return 0;
    }

    /* Students */
    for (i = 0; i < out->total; i++) {
        size_t len;

        if (fgets(line, sizeof line, fp) == NULL) {
            return 0;
        }
        line[strcspn(line, "\r\n")] = '\0';
        len = strlen(line);
        if (len == 0 || len >= NAME_SIZE) {
            return 0;
        }
        strcpy(out->students[i].name, line);

        if (fgets(line, sizeof line, fp) == NULL) {
            return 0;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) != MAX_DAYS) {
            return 0;
        }
        for (d = 0; d < MAX_DAYS; d++) {
            char ch = line[d];
            if (ch != 'P' && ch != 'A' && ch != 'L' && ch != '-') {
                return 0;
            }
            out->students[i].days[d] = ch;
        }
    }
    return 1;
}

static void loadFile(ClassData *c) {
    char fileName[80];
    FILE *fp;
    ClassData loaded;
    int ok;

    buildFileName(c, fileName, sizeof fileName);
    fp = fopen(fileName, "r");
    if (fp == NULL) {
        printf("No saved file for %s yet. Starting a new one.\n", c->section);
        return;
    }

    memset(&loaded, 0, sizeof loaded);
    ok = parseFile(fp, &loaded);
    fclose(fp);

    if (!ok) {
        /* Our current data is untouched, so nothing gets corrupted */
        printf("The save file %s is damaged or in an old format. "
               "Starting fresh.\n", fileName);
        return;
    }

    loaded.dirty = 0;
    *c = loaded;
    printf("Loaded %d student(s) for %s (%s %d).\n", c->total, c->section,
           MONTH_NAMES[c->month - 1], c->year);
}

/* ---------- Menu and main ---------- */

static void initClass(ClassData *c) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    memset(c, 0, sizeof *c);
    if (t != NULL) {
        c->month = t->tm_mon + 1;
        c->year = t->tm_year + 1900;
    } else {
        c->month = 1;
        c->year = 2026;
    }
}

static void printMenu(const ClassData *c) {
    printf("\n--- ATTENDANCE MENU (%s, %s %d%s) ---\n", c->section,
           MONTH_NAMES[c->month - 1], c->year,
           c->dirty ? ", unsaved changes" : "");
    printf("1. Add student\n");
    printf("2. Edit student name\n");
    printf("3. Delete student\n");
    printf("4. Mark attendance\n");
    printf("5. Show one student\n");
    printf("6. Show whole class\n");
    printf("7. Search student by name\n");
    printf("8. Change month (clears marks)\n");
    printf("9. Save\n");
    printf("0. Exit\n");
}

/* Returns 1 if the program may exit */
static int confirmExit(ClassData *c) {
    if (!c->dirty) {
        return 1;
    }
    if (inputClosed) {
        printf("\nInput closed. Saving your changes before exit.\n");
        saveFile(c);
        return 1;
    }
    if (readYesNo("You have unsaved changes. Save before exit? (y/n): ")) {
        return saveFile(c);          /* stay open if saving failed */
    }
    return 1;
}

void attendanceMenu(void) {
    ClassData cls;
    int choice;
    int running = 1;

    initClass(&cls);

    /* The class comes from the Class Schedule module */
    if (!chooseClass(cls.section)) {
        printf("Returning to the main menu.\n");
        return;
    }
    printf("\nRecording attendance for: %s\n", cls.section);
    printClassInfo(cls.section);
    loadFile(&cls);

    while (running) {
        printMenu(&cls);

        if (!readIntInRange("Choice: ", 0, 9, &choice)) {
            if (!inputClosed) {
                continue;            /* bad input: show the menu again */
            }
            choice = 0;              /* input ended: leave the program */
        }

        switch (choice) {
            case 1: addStudent(&cls);       break;
            case 2: editStudent(&cls);      break;
            case 3: deleteStudent(&cls);    break;
            case 4: markAttendance(&cls);   break;
            case 5: showStudent(&cls);      break;
            case 6: showClass(&cls);        break;
            case 7: searchStudents(&cls);   break;
            case 8: changeMonth(&cls);      break;
            case 9: saveFile(&cls);         break;
            case 0:
                if (confirmExit(&cls)) {
                    running = 0;
                }
                break;
        }
    }

    printf("Returning to the main menu.\n");
}
