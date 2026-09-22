#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define NAME_LEN 50
#define BG_LEN 4
#define DATE_LEN 11
#define HASH_SIZE 101
#define MAX_HOSPITALS 20

/* ====================== STRUCTURES ====================== */

typedef struct Donor {
    int id;
    char name[NAME_LEN];
    char bloodGroup[BG_LEN];
    char lastDonation[DATE_LEN];
    struct Donor *next;
} Donor;

typedef struct Request {
    int id;
    char patient[NAME_LEN];
    char bloodGroup[BG_LEN];
    int units;
    int priority;               // 1-10 (higher = more urgent)
    struct Request *next;
} Request;

typedef struct Edge {
    int to;
    int cost;
    struct Edge *next;
} Edge;

typedef struct {
    int id;
    char name[NAME_LEN];
    char lastDonation[DATE_LEN];
} UndoRecord;

/* ====================== GLOBALS ====================== */

Donor *donorHead = NULL;
Donor *hashTable[HASH_SIZE] = {NULL};

Request *queueFront = NULL, *queueRear = NULL;   // FIFO queue
Request *heap[100];                              // Max-Heap
int heapSize = 0;
Edge *graph[MAX_HOSPITALS] = {NULL};
char hospitalNames[MAX_HOSPITALS][NAME_LEN];
int hospitalCount = 0;

UndoRecord undoStack[50];
int undoTop = -1;

char todayDate[DATE_LEN];

/* ====================== UTILITY ====================== */

int hash(int id) {
    return id % HASH_SIZE;
}

int daysBetween(const char *d1, const char *d2) {
    // Very simple day difference (assumes YYYY-MM-DD and same year for demo)
    int y1, m1, day1, y2, m2, day2;
    sscanf(d1, "%d-%d-%d", &y1, &m1, &day1);
    sscanf(d2, "%d-%d-%d", &y2, &m2, &day2);
    return (y2 - y1) * 365 + (m2 - m1) * 30 + (day2 - day1);
}

int isEligible(Donor *d) {
    return daysBetween(d->lastDonation, todayDate) >= 90;
}
/* ====================== 1. DONOR LINKED LIST + HASH ====================== */

void registerDonor(int id, const char *name, const char *bg, const char *date) {
    if (hashTable[hash(id)] != NULL) {
        // simple check – in real code you should walk the chain
        printf("ID already exists!\n");
        return;
    }

    Donor *d = (Donor*)malloc(sizeof(Donor));
    d->id = id;
    strcpy(d->name, name);
    strcpy(d->bloodGroup, bg);
    strcpy(d->lastDonation, date);
    d->next = donorHead;
    donorHead = d;
    // Hash table stores pointer for O(1) lookup
    hashTable[hash(id)] = d;

    printf("Donor registered successfully.\n");
}
void editDonor(int id, const char *newName, const char *newDate) {
    Donor *d = hashTable[hash(id)];
    if (!d || d->id != id) {
        printf("Donor not found.\n");
        return;
    }

    // Save for undo
    if (undoTop < 49) {
        undoTop++;
        undoStack[undoTop].id = id;
        strcpy(undoStack[undoTop].name, d->name);
        strcpy(undoStack[undoTop].lastDonation, d->lastDonation);
    }

    strcpy(d->name, newName);
    strcpy(d->lastDonation, newDate);
    printf("Donor updated.\n");
}

void undoLastEdit() {
    if (undoTop < 0) {
        printf("Nothing to undo.\n");
        return;
    }
    UndoRecord u = undoStack[undoTop--];
    Donor *d = hashTable[hash(u.id)];
    if (d && d->id == u.id) {
        strcpy(d->name, u.name);
        strcpy(d->lastDonation, u.lastDonation);
        printf("Undo successful.\n");
    }
}

void displayDonorsByGroup(const char *bg) {
    Donor *curr = donorHead;
    int found = 0;
    while (curr) {
        if (strcmp(curr->bloodGroup, bg) == 0) {
            printf("ID: %d | %s | Last: %s\n", curr->id, curr->name, curr->lastDonation);
            found = 1;
        }
        curr = curr->next;
    }
    if (!found) printf("No donors of group %s.\n", bg);
}

/* ====================== 2. SEARCH BY HASH ====================== */

void searchByHash(int id) {
    Donor *d = hashTable[hash(id)];
    if (d && d->id == id) {
        printf("Found -> ID: %d | %s | %s | Last: %s\n",
               d -> id, d->name, d->bloodGroup, d->lastDonation);
    } else {
        printf("Donor not found.\n");
    }
}
/* ====================== 3. QUEUE (FIFO) ====================== */

void enqueueRequest(int id, const char *patient, const char *bg, int units, int priority) {
    Request *r = (Request*)malloc(sizeof(Request));
    r->id = id;
    strcpy(r->patient, patient);
    strcpy(r->bloodGroup, bg);
    r->units = units;
    r->priority = priority;
    r->next = NULL;

    if (!queueRear) {
        queueFront = queueRear = r;
    } else {
        queueRear->next = r;
        queueRear = r;
    }
    printf("Request added to FIFO queue.\n");
}

void processNextRequest() {
    if (!queueFront) {
        printf("No requests in queue.\n");
        return;
    }
    Request *r = queueFront;
    printf("Processing FIFO → Patient: %s | BG: %s | Units: %d\n",
           r->patient, r->bloodGroup, r->units);
    queueFront = queueFront->next;
    if (!queueFront) queueRear = NULL;
    free(r);
}

/* ====================== 4. MAX-HEAP ====================== */

void heapifyUp(int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (heap[p]->priority >= heap[i]->priority) break;
        Request *tmp = heap[p];
        heap[p] = heap[i];
        heap[i] = tmp;
        i = p;
    }
}

void heapifyDown(int i) {
    while (1) {
        int largest = i;
        int l = 2 * i + 1, r = 2 * i + 2;
        if (l < heapSize && heap[l]->priority > heap[largest]->priority) largest = l;
        if (r < heapSize && heap[r]->priority > heap[largest]->priority) largest = r;
        if (largest == i) break;
        Request *tmp = heap[i];
        heap[i] = heap[largest];
        heap[largest] = tmp;
        i = largest;
    }
}

void insertHeap(Request *r) {
    heap[heapSize] = r;
    heapifyUp(heapSize);
    heapSize++;
}

Request* extractMax() {
    if (heapSize == 0) return NULL;
    Request *max = heap[0];
    heap[0] = heap[--heapSize];
    heapifyDown(0);
    return max;
}

void processTopPriorityRequest() {
    Request *r = extractMax();
    if (!r) {
        printf("No priority requests.\n");
        return;
    }
    printf("Processing URGENT → Patient: %s | Priority: %d | BG: %s\n",
           r->patient, r->priority, r->bloodGroup);
    free(r);
}

/* ====================== 5 & 6. GRAPH + DIJKSTRA ====================== */

void addHospital(const char *name) {
    if (hospitalCount >= MAX_HOSPITALS) return;
    strcpy(hospitalNames[hospitalCount], name);
    hospitalCount++;
    printf("Hospital added (index %d).\n", hospitalCount - 1);
}

void addEdge(int u, int v, int cost) {
    Edge *e1 = (Edge*)malloc(sizeof(Edge));
    e1->to = v; e1->cost = cost; e1->next = graph[u];
    graph[u] = e1;

    Edge *e2 = (Edge*)malloc(sizeof(Edge));
    e2->to = u; e2->cost = cost; e2->next = graph[v];
    graph[v] = e2;
    printf("Route added.\n");
}

void dijkstra(int src) {
    int dist[MAX_HOSPITALS];
    int visited[MAX_HOSPITALS] = {0};

    for (int i = 0; i < hospitalCount; i++) dist[i] = INT_MAX;
    dist[src] = 0;

    for (int count = 0; count < hospitalCount - 1; count++) {
        int u = -1, min = INT_MAX;
        for (int i = 0; i < hospitalCount; i++)
            if (!visited[i] && dist[i] < min) { min = dist[i]; u = i; }
        if (u == -1) break;
        visited[u] = 1;

        for (Edge *e = graph[u]; e; e = e->next) {
            if (!visited[e->to] && dist[u] != INT_MAX &&
                dist[u] + e->cost < dist[e->to]) {
                dist[e->to] = dist[u] + e->cost;
            }
        }
    }

    printf("\nNearest hospitals from %s:\n", hospitalNames[src]);
    for (int i = 0; i < hospitalCount; i++) {
        if (i == src) continue;
        if (dist[i] == INT_MAX)
            printf("  %s → unreachable\n", hospitalNames[i]);
        else
            printf("  %s → cost %d\n", hospitalNames[i], dist[i]);
    }
}

/* ====================== 7. QUICK SORT ====================== */

void swapDonors(Donor **a, Donor **b) {
    Donor *t = *a; *a = *b; *b = t;
}

int partition(Donor **arr, int low, int high, int byName) {
    Donor *pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        int cmp;
        if (byName)
            cmp = strcmp(arr[j]->name, pivot->name);
        else
            cmp = daysBetween(arr[j]->lastDonation, todayDate) -
                  daysBetween(pivot->lastDonation, todayDate);

        if (cmp > 0) {          // most overdue first OR alphabetical
            i++;
            swapDonors(&arr[i], &arr[j]);
        }
    }
    swapDonors(&arr[i + 1], &arr[high]);
    return i + 1;
}

void quickSort(Donor **arr, int low, int high, int byName) {
    if (low < high) {
        int pi = partition(arr, low, high, byName);
        quickSort(arr, low, pi - 1, byName);
        quickSort(arr, pi + 1, high, byName);
    }
}

void showEligibleDonorsSortedByWait() {
    Donor *temp[200];
    int n = 0;
    for (Donor *d = donorHead; d; d = d->next)
        if (isEligible(d)) temp[n++] = d;

    if (n == 0) {
        printf("No eligible donors.\n");
        return;
    }

    quickSort(temp, 0, n - 1, 0);   // by wait time (most overdue first)

    printf("\nEligible donors (most overdue first):\n");
    for (int i = 0; i < n; i++)
        printf("%s | %s | Wait days: %d\n",
               temp[i]->name, temp[i]->bloodGroup,
               daysBetween(temp[i]->lastDonation, todayDate));
}

void showAlphabeticalReport() {
    Donor *temp[200];
    int n = 0;
    for (Donor *d = donorHead; d; d = d->next) temp[n++] = d;

    if (n == 0) {
        printf("No donors.\n");
        return;
    }

    quickSort(temp, 0, n - 1, 1);   // by name

    printf("\nAlphabetical Donor Report:\n");
    for (int i = 0; i < n; i++)
        printf("%s | ID: %d | %s\n", temp[i]->name, temp[i]->id, temp[i]->bloodGroup);
}

/* ====================== MAIN MENU ====================== */
int main() {
    printf("Enter today's date (YYYY-MM-DD): ");
    scanf("%10s", todayDate);

    int choice, id, units, priority, u, v, w, src;
    char name[NAME_LEN], bg[BG_LEN], date[DATE_LEN], patient[NAME_LEN];

    // Menu is printed ONLY ONCE
    printf("\n===== Blood Donor Finder (Minimal Version) =====\n");
    printf(" 1. Register Donor\n");
    printf(" 2. Edit Donor\n");
    printf(" 3. Undo Last Edit\n");
    printf(" 4. Display Donors by Blood Group\n");
    printf(" 5. Add Hospital\n");
    printf(" 6. Add Route Between Hospitals\n");
    printf(" 7. New Emergency Request (FIFO + Priority)\n");
    printf(" 8. Process Next Request (FIFO)\n");
    printf(" 9. Process Most Urgent Request (Heap)\n");
    printf("10. Search Donor by ID (Hash)\n");
    printf("11. Show Eligible Donors (Quick Sort)\n");
    printf("12. Alphabetical Report (Quick Sort)\n");
    printf("13. Find Nearest Hospital (Dijkstra)\n");
    printf(" 0. Exit\n");

    do {
        printf("\nChoice: ");
        if (scanf("%d", &choice) != 1) break;

        switch (choice) {
            case 1:
                printf("ID: "); scanf("%d", &id);
                printf("Name: "); scanf(" %49[^\n]", name);
                printf("Blood Group: "); scanf("%3s", bg);
                printf("Last Donation (YYYY-MM-DD): "); scanf("%10s", date);
                registerDonor(id, name, bg, date);
                break;

            case 2:
                printf("Donor ID: "); scanf("%d", &id);
                printf("New Name: "); scanf(" %49[^\n]", name);
                printf("New Last Donation: "); scanf("%10s", date);
                editDonor(id, name, date);
                break;

            case 3:
                undoLastEdit();
                break;

            case 4:
                printf("Blood Group: "); scanf("%3s", bg);
                displayDonorsByGroup(bg);
                break;

            case 5:
                printf("Hospital Name: "); scanf(" %49[^\n]", name);
                addHospital(name);
                break;

            case 6:
                if (hospitalCount < 2) {
                    printf("Add at least 2 hospitals first.\n");
                    break;
                }
                printf("Current hospitals:\n");
                for (int i = 0; i < hospitalCount; i++)
                    printf("  %d = %s\n", i, hospitalNames[i]);
                printf("From: "); scanf("%d", &u);
                printf("To: "); scanf("%d", &v);
                printf("Cost: "); scanf("%d", &w);
                addEdge(u, v, w);
                break;

            case 7:
                printf("Request ID: "); scanf("%d", &id);
                printf("Patient: "); scanf(" %49[^\n]", patient);
                printf("Blood Group: "); scanf("%3s", bg);
                printf("Units: "); scanf("%d", &units);
                printf("Priority (1-10): "); scanf("%d", &priority);
                enqueueRequest(id, patient, bg, units, priority);

                // also put a copy in the heap
                {
                    Request *r = (Request*)malloc(sizeof(Request));
                    r->id = id;
                    strcpy(r->patient, patient);
                    strcpy(r->bloodGroup, bg);
                    r->units = units;
                    r->priority = priority;
                    insertHeap(r);
                }
                break;

            case 8:
                processNextRequest();
                break;

            case 9:
                processTopPriorityRequest();
                break;

            case 10:
                printf("Donor ID: "); scanf("%d", &id);
                searchByHash(id);
                break;

            case 11:
                showEligibleDonorsSortedByWait();
                break;

            case 12:
                showAlphabeticalReport();
                break;

            case 13:
                if (hospitalCount == 0) {
                    printf("No hospitals yet.\n");
                    break;
                }
                printf("Current hospitals:\n");
                for (int i = 0; i < hospitalCount; i++)
                    printf("  %d = %s\n", i, hospitalNames[i]);
                printf("Source index: "); scanf("%d", &src);
                dijkstra(src);
                break;

            case 0:
                printf("Goodbye!\n");
                break;

            default:
                printf("Invalid choice.\n");
        }
    } while (choice != 0);

    return 0;
}