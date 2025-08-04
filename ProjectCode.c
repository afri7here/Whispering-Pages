#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define TOTAL_CHAIRS 10
#define TOTAL_TABLES 5
#define TOTAL_AV_EQUIPMENT 3
#define MAX_GROUPS 10
#define MAX_ROOMS 10
#define MAX_BOOKS 100
#define MAX_HOURS 24

typedef struct {
    char title[50];
    sem_t available_sem;
    int total_copies;
} Book;

typedef struct {
    int id;
    int chairs_needed;
    int tables_needed;
    int av_needed;
    pthread_t thread;
    int room_id;
    char book_title[50];
    int start_hour;
    int end_hour;
} Group;

typedef struct {
    int id;
    int hour_availability[MAX_HOURS];
    pthread_mutex_t hour_mutex[MAX_HOURS];
    pthread_mutex_t room_mutex;
} Room;

// Global resources
Room rooms[MAX_ROOMS];
int num_rooms;
int available_chairs = TOTAL_CHAIRS;
int available_tables = TOTAL_TABLES;
sem_t av_equipment_sem;
Book catalog[MAX_BOOKS];
int num_books = 3;

// Synchronization
pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t chair_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t table_cond = PTHREAD_COND_INITIALIZER;
pthread_rwlock_t catalog_rwlock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t book_selection_mutex = PTHREAD_MUTEX_INITIALIZER;

void initialize_catalog() {
    strcpy(catalog[0].title, "Heart Bones");
    sem_init(&catalog[0].available_sem, 0, 5);
    catalog[0].total_copies = 5;

    strcpy(catalog[1].title, "The Alchemist");
    sem_init(&catalog[1].available_sem, 0, 3);
    catalog[1].total_copies = 3;

    strcpy(catalog[2].title, "Kafka on the Shore");
    sem_init(&catalog[2].available_sem, 0, 4);
    catalog[2].total_copies = 4;
}

void display_books(int group_id) {
    pthread_rwlock_rdlock(&catalog_rwlock);
    printf("\n=== GROUP %d BOOK SELECTION ===\n", group_id);
    printf("Available Books:\n");
    printf("----------------\n");
    for(int i = 0; i < num_books; i++) {
        int available;
        sem_getvalue(&catalog[i].available_sem, &available);
        printf("%d. %s (Available: %d/%d)\n",
              i+1, catalog[i].title, available, catalog[i].total_copies);
    }
    printf("----------------\n");
    printf("Group %d: Choose a book to borrow (1-%d): ", group_id, num_books);
    fflush(stdout);
    pthread_rwlock_unlock(&catalog_rwlock);
}

int find_book_index(const char* title) {
    pthread_rwlock_rdlock(&catalog_rwlock);
    for(int i = 0; i < num_books; i++) {
        if(strcmp(catalog[i].title, title) == 0) {
            pthread_rwlock_unlock(&catalog_rwlock);
            return i;
        }
    }
    pthread_rwlock_unlock(&catalog_rwlock);
    return -1;
}

void initialize_rooms() {
    for (int i = 0; i < num_rooms; i++) {
        rooms[i].id = i;
        pthread_mutex_init(&rooms[i].room_mutex, NULL);
        for (int j = 0; j < MAX_HOURS; j++) {
            rooms[i].hour_availability[j] = 0;
            pthread_mutex_init(&rooms[i].hour_mutex[j], NULL);
        }
    }
}

int book_time_slot(int room_id, int start, int end) {
    pthread_mutex_lock(&rooms[room_id].room_mutex);

    for (int i = start; i < end; i++) {
        pthread_mutex_lock(&rooms[room_id].hour_mutex[i]);
        if (rooms[room_id].hour_availability[i]) {
            for (int j = start; j <= i; j++) {
                pthread_mutex_unlock(&rooms[room_id].hour_mutex[j]);
            }
            pthread_mutex_unlock(&rooms[room_id].room_mutex);
            return 0;
        }
    }

    for (int i = start; i < end; i++) {
        rooms[room_id].hour_availability[i] = 1;
        pthread_mutex_unlock(&rooms[room_id].hour_mutex[i]);
    }

    pthread_mutex_unlock(&rooms[room_id].room_mutex);
    return 1;
}

void release_time_slot(int room_id, int start, int end) {
    pthread_mutex_lock(&rooms[room_id].room_mutex);
    for (int i = start; i < end; i++) {
        pthread_mutex_lock(&rooms[room_id].hour_mutex[i]);
        rooms[room_id].hour_availability[i] = 0;
        pthread_mutex_unlock(&rooms[room_id].hour_mutex[i]);
    }
    pthread_mutex_unlock(&rooms[room_id].room_mutex);
}

int compare_groups(const void* a, const void* b) {
    Group* groupA = (Group*)a;
    Group* groupB = (Group*)b;
    return groupA->start_hour - groupB->start_hour;
}

void* group_activity(void* arg) {
    Group* group = (Group*)arg;

    // Wait until it's the group's scheduled time
    time_t now;
    time(&now);
    struct tm *tm_now = localtime(&now);
    int current_hour = tm_now->tm_hour;

    while(current_hour < group->start_hour) {
        sleep(1);
        time(&now);
        tm_now = localtime(&now);
        current_hour = tm_now->tm_hour;
    }

    // Try to find an available room
    int room_found = 0;
    for (int i = 0; i < num_rooms; i++) {
        if (book_time_slot(i, group->start_hour, group->end_hour)) {
            group->room_id = i;
            room_found = 1;
            break;
        }
    }

    if (!room_found) {
        pthread_mutex_lock(&output_mutex);
        printf("\nGroup %d couldn't find available room for %d:00-%d:00\n",
              group->id, group->start_hour, group->end_hour);
        pthread_mutex_unlock(&output_mutex);
        return NULL;
    }

    pthread_mutex_lock(&output_mutex);
    printf("\nGroup %d booked Room %d from %d:00-%d:00\n",
          group->id, group->room_id, group->start_hour, group->end_hour);
    pthread_mutex_unlock(&output_mutex);

    // Resource allocation
    pthread_mutex_lock(&resource_mutex);

    // Allocate chairs
    while (available_chairs < group->chairs_needed) {
        pthread_mutex_lock(&output_mutex);
        printf("Group %d waiting for chairs (needs %d, available %d)...\n",
              group->id, group->chairs_needed, available_chairs);
        pthread_mutex_unlock(&output_mutex);
        pthread_cond_wait(&chair_cond, &resource_mutex);
    }
    available_chairs -= group->chairs_needed;
    pthread_mutex_lock(&output_mutex);
    printf("Group %d took %d chairs. Remaining: %d\n",
          group->id, group->chairs_needed, available_chairs);
    pthread_mutex_unlock(&output_mutex);

    // Allocate tables
    while (available_tables < group->tables_needed) {
        pthread_mutex_lock(&output_mutex);
        printf("Group %d waiting for tables (needs %d, available %d)...\n",
              group->id, group->tables_needed, available_tables);
        pthread_mutex_unlock(&output_mutex);
        pthread_cond_wait(&table_cond, &resource_mutex);
    }
    available_tables -= group->tables_needed;
    pthread_mutex_lock(&output_mutex);
    printf("Group %d took %d tables. Remaining: %d\n",
          group->id, group->tables_needed, available_tables);
    pthread_mutex_unlock(&output_mutex);

    pthread_mutex_unlock(&resource_mutex);

    // Allocate AV equipment
    if (group->av_needed) {
        if (sem_trywait(&av_equipment_sem) == 0) {
            pthread_mutex_lock(&output_mutex);
            printf("Group %d took AV equipment.\n", group->id);
            pthread_mutex_unlock(&output_mutex);
        } else {
            pthread_mutex_lock(&output_mutex);
            printf("Group %d couldn't get AV equipment.\n", group->id);
            pthread_mutex_unlock(&output_mutex);
            group->av_needed = 0;
        }
    }

    // Book selection with proper synchronization
void handle_book_selection(Group* group) {
    // Lock the entire book selection process
    pthread_mutex_lock(&output_mutex);

    printf("\n=== GROUP %d BOOK SELECTION ===\n", group->id);

    // Display available books
    pthread_rwlock_rdlock(&catalog_rwlock);
    printf("Available Books:\n");
    printf("----------------\n");
    for(int i = 0; i < num_books; i++) {
        int available;
        sem_getvalue(&catalog[i].available_sem, &available);
        printf("%d. %s (Available: %d/%d)\n",
              i+1, catalog[i].title, available, catalog[i].total_copies);
    }
    printf("----------------\n");
    pthread_rwlock_unlock(&catalog_rwlock);

    int book_choice = 0;
    int valid_input = 0;

    while (!valid_input) {
        printf("Group %d: Choose a book to borrow (1-%d): ", group->id, num_books);
        fflush(stdout);

        // Get input with input mutex
        pthread_mutex_lock(&input_mutex);
        if (scanf("%d", &book_choice) == 1) {
            if (book_choice >= 1 && book_choice <= num_books) {
                valid_input = 1;
            } else {
                printf("Invalid choice! Please enter 1-%d\n", num_books);
            }
        } else {
            // Clear invalid input
            while (getchar() != '\n');
            printf("Invalid input! Please enter a number\n");
        }
        pthread_mutex_unlock(&input_mutex);
    }

    // Process the selection
    int book_idx = book_choice - 1;
    if (sem_trywait(&catalog[book_idx].available_sem) == 0) {
        strcpy(group->book_title, catalog[book_idx].title);
        int available;
        sem_getvalue(&catalog[book_idx].available_sem, &available);
        printf("Group %d borrowed '%s'. Copies left: %d/%d\n\n",
              group->id, group->book_title, available, catalog[book_idx].total_copies);
    } else {
        printf("Group %d couldn't borrow '%s' (no copies available)\n\n",
              group->id, catalog[book_idx].title);
        strcpy(group->book_title, "None");
    }

    pthread_mutex_unlock(&output_mutex);
}

// In the group_activity function, replace the book selection code with:
handle_book_selection(group);
    // Simulate meeting
    int duration = group->end_hour - group->start_hour;
    if (duration < 1) duration = 1;
    sleep(duration);

    // Resource release
    if (strcmp(group->book_title, "None") != 0) {
        int return_idx = find_book_index(group->book_title);
        if (return_idx != -1) {
            sem_post(&catalog[return_idx].available_sem);
            int available;
            sem_getvalue(&catalog[return_idx].available_sem, &available);
            pthread_mutex_lock(&output_mutex);
            printf("Group %d returned '%s' at %d:00. Copies now: %d/%d\n",
                  group->id, group->book_title, group->end_hour,
                  available, catalog[return_idx].total_copies);
            pthread_mutex_unlock(&output_mutex);
        }
    }

    if (group->av_needed) {
        sem_post(&av_equipment_sem);
        pthread_mutex_lock(&output_mutex);
        printf("Group %d returned AV equipment.\n", group->id);
        pthread_mutex_unlock(&output_mutex);
    }

    pthread_mutex_lock(&resource_mutex);
    available_tables += group->tables_needed;
    available_chairs += group->chairs_needed;
    pthread_mutex_lock(&output_mutex);
    printf("\nGroup %d released %d chairs and %d tables at %d:00.\n",
          group->id, group->chairs_needed, group->tables_needed, group->end_hour);
    pthread_mutex_unlock(&output_mutex);

    release_time_slot(group->room_id, group->start_hour, group->end_hour);
    pthread_mutex_lock(&output_mutex);
    printf("Group %d left Room %d at %d:00\n",
          group->id, group->room_id, group->end_hour);
    pthread_mutex_unlock(&output_mutex);

    pthread_cond_broadcast(&chair_cond);
    pthread_cond_broadcast(&table_cond);
    pthread_mutex_unlock(&resource_mutex);

    return NULL;
}

void cleanup() {
    for (int i = 0; i < num_books; i++) {
        sem_destroy(&catalog[i].available_sem);
    }
    sem_destroy(&av_equipment_sem);

    pthread_mutex_destroy(&resource_mutex);
    pthread_cond_destroy(&chair_cond);
    pthread_cond_destroy(&table_cond);
    pthread_rwlock_destroy(&catalog_rwlock);
    pthread_mutex_destroy(&input_mutex);
    pthread_mutex_destroy(&output_mutex);
    pthread_mutex_destroy(&book_selection_mutex);

    for (int i = 0; i < num_rooms; i++) {
        pthread_mutex_destroy(&rooms[i].room_mutex);
        for (int j = 0; j < MAX_HOURS; j++) {
            pthread_mutex_destroy(&rooms[i].hour_mutex[j]);
        }
    }
}

int main() {
    initialize_catalog();
    sem_init(&av_equipment_sem, 0, TOTAL_AV_EQUIPMENT);
    int num_groups;

    printf("Enter number of rooms (1-%d): ", MAX_ROOMS);
    scanf("%d", &num_rooms);
    while(num_rooms < 1 || num_rooms > MAX_ROOMS) {
        printf("Invalid input! Enter rooms (1-%d): ", MAX_ROOMS);
        scanf("%d", &num_rooms);
    }
    initialize_rooms();

    printf("Enter number of groups (1-%d): ", MAX_GROUPS);
    scanf("%d", &num_groups);
    while(num_groups < 1 || num_groups > MAX_GROUPS) {
        printf("Invalid input! Enter groups (1-%d): ", MAX_GROUPS);
        scanf("%d", &num_groups);
    }

    Group* groups = malloc(num_groups * sizeof(Group));

    for(int i = 0; i < num_groups; i++) {
        groups[i].id = i+1;
        printf("\nGroup %d details:\n", i+1);

        printf("Chairs needed (1-%d): ", TOTAL_CHAIRS);
        scanf("%d", &groups[i].chairs_needed);
        while(groups[i].chairs_needed < 1 || groups[i].chairs_needed > TOTAL_CHAIRS) {
            printf("Invalid number! Enter (1-%d): ", TOTAL_CHAIRS);
            scanf("%d", &groups[i].chairs_needed);
        }

        printf("Tables needed (1-%d): ", TOTAL_TABLES);
        scanf("%d", &groups[i].tables_needed);
        while(groups[i].tables_needed < 1 || groups[i].tables_needed > TOTAL_TABLES) {
            printf("Invalid number! Enter (1-%d): ", TOTAL_TABLES);
            scanf("%d", &groups[i].tables_needed);
        }

        printf("Need AV equipment? (1=Yes, 0=No): ");
        scanf("%d", &groups[i].av_needed);
        while(groups[i].av_needed != 0 && groups[i].av_needed != 1) {
            printf("Invalid input! Enter (1=Yes, 0=No): ");
            scanf("%d", &groups[i].av_needed);
        }

        printf("Start hour (0-23): ");
        scanf("%d", &groups[i].start_hour);
        while(groups[i].start_hour < 0 || groups[i].start_hour > 23) {
            printf("Invalid hour! Enter (0-23): ");
            scanf("%d", &groups[i].start_hour);
        }

        printf("End hour (%d-24): ", groups[i].start_hour + 1);
        scanf("%d", &groups[i].end_hour);
        while(groups[i].end_hour <= groups[i].start_hour || groups[i].end_hour > 24) {
            printf("Invalid end hour! Enter (%d-24): ", groups[i].start_hour + 1);
            scanf("%d", &groups[i].end_hour);
        }
    }

    // Sort groups by start time
    qsort(groups, num_groups, sizeof(Group), compare_groups);

    printf("\nGroup Schedule (in chronological order):\n");
    for(int i = 0; i < num_groups; i++) {
        printf("Group %d: %02d:00-%02d:00\n",
              groups[i].id, groups[i].start_hour, groups[i].end_hour);
    }

    printf("\nStarting simulation...\n");
    for(int i = 0; i < num_groups; i++) {
        pthread_create(&groups[i].thread, NULL, group_activity, &groups[i]);
        usleep(100000);  // Small delay between group creation
    }

    for(int i = 0; i < num_groups; i++) {
        pthread_join(groups[i].thread, NULL);
    }

    cleanup();
    free(groups);

    printf("\nFinal Book Status:\n");
    pthread_rwlock_rdlock(&catalog_rwlock);
    printf("Available Books:\n");
    printf("----------------\n");
    for(int i = 0; i < num_books; i++) {
        int available;
        sem_getvalue(&catalog[i].available_sem, &available);
        printf("%d. %s (Available: %d/%d)\n",
              i+1, catalog[i].title, available, catalog[i].total_copies);
    }
    printf("----------------\n");
    pthread_rwlock_unlock(&catalog_rwlock);

    printf("\nAll book club meetings have concluded.\n");
    return 0;
}
