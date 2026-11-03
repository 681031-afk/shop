/*
 * Inventory Management System (C)
 * -------------------------------------------------
 * Menus : Dashboard, Inventory List, Stock In, Stock Out,
 *         Alerts & Notifications, Reorder Thresholds, Activity Log
 * Alerts: Stock Restocked / Stock Reduced / Low Stock / Out of Stock
 * Data  : saved automatically to inventory.dat and activity.dat
 *
 * Build : gcc -Wall -o inventory inventory.c
 * Run   : ./inventory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ITEMS         200
#define MAX_LOGS          1000
#define MAX_NOTIFS        100
#define NAME_LEN          50
#define NOTIF_LEN         512
#define DEFAULT_THRESHOLD 10
#define ITEM_FILE         "inventory.dat"
#define LOG_FILE          "activity.dat"

/* ---------- Data structures ---------- */

typedef struct {
    int    id;
    char   name[NAME_LEN];
    int    qty;
    int    threshold;
    double price;
} Item;

typedef struct {
    char time[20];
    char type[16];          /* STOCK IN / STOCK OUT / NEW ITEM / THRESHOLD */
    char item[NAME_LEN];
    int  previous;
    int  change;
    int  current;
} LogEntry;

typedef struct {
    char time[20];
    char text[NOTIF_LEN];
} Notification;

static Item         items[MAX_ITEMS];
static int          item_count = 0;
static int          next_id    = 1;

static LogEntry     logs[MAX_LOGS];
static int          log_count  = 0;

static Notification notifs[MAX_NOTIFS];
static int          notif_count = 0;

/* ---------- Input helpers ---------- */

static void read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    if (!fgets(buf, (int)size, stdin)) {
        buf[0] = '\0';
        return;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

/* Returns 1 on valid integer in [min, max], 0 otherwise */
static int read_int(const char *prompt, int min, int max, int *out)
{
    char buf[64], *end;
    long v;

    read_line(prompt, buf, sizeof buf);
    if (buf[0] == '\0') return 0;
    v = strtol(buf, &end, 10);
    if (*end != '\0' || v < min || v > max) return 0;
    *out = (int)v;
    return 1;
}

static int read_double(const char *prompt, double *out)
{
    char buf[64], *end;
    double v;

    read_line(prompt, buf, sizeof buf);
    if (buf[0] == '\0') return 0;
    v = strtod(buf, &end);
    if (*end != '\0' || v < 0) return 0;
    *out = v;
    return 1;
}

static void pause_screen(void)
{
    char buf[8];
    read_line("\nPress Enter to continue...", buf, sizeof buf);
}

static void now_str(char *out, size_t size)
{
    time_t t = time(NULL);
    strftime(out, size, "%Y-%m-%d %H:%M:%S", localtime(&t));
}

/* ---------- Persistence ---------- */

static void save_all(void)
{
    FILE *f = fopen(ITEM_FILE, "w");
    int i;
    if (f) {
        fprintf(f, "%d\n", next_id);
        for (i = 0; i < item_count; i++)
            fprintf(f, "%d|%s|%d|%d|%.2f\n", items[i].id, items[i].name,
                    items[i].qty, items[i].threshold, items[i].price);
        fclose(f);
    }
    f = fopen(LOG_FILE, "w");
    if (f) {
        for (i = 0; i < log_count; i++)
            fprintf(f, "%s|%s|%s|%d|%d|%d\n", logs[i].time, logs[i].type,
                    logs[i].item, logs[i].previous, logs[i].change,
                    logs[i].current);
        fclose(f);
    }
}

static void load_all(void)
{
    char line[256];
    FILE *f = fopen(ITEM_FILE, "r");

    if (f) {
        if (fgets(line, sizeof line, f)) next_id = atoi(line);
        while (item_count < MAX_ITEMS && fgets(line, sizeof line, f)) {
            Item *it = &items[item_count];
            char *p = strtok(line, "|");
            if (!p) continue;
            it->id = atoi(p);
            if (!(p = strtok(NULL, "|"))) continue;
            strncpy(it->name, p, NAME_LEN - 1);
            it->name[NAME_LEN - 1] = '\0';
            if (!(p = strtok(NULL, "|"))) continue;
            it->qty = atoi(p);
            if (!(p = strtok(NULL, "|"))) continue;
            it->threshold = atoi(p);
            if (!(p = strtok(NULL, "|\n"))) continue;
            it->price = atof(p);
            item_count++;
        }
        fclose(f);
    }

    f = fopen(LOG_FILE, "r");
    if (f) {
        while (log_count < MAX_LOGS && fgets(line, sizeof line, f)) {
            LogEntry *lg = &logs[log_count];
            char *p = strtok(line, "|");
            if (!p) continue;
            strncpy(lg->time, p, sizeof lg->time - 1);
            lg->time[sizeof lg->time - 1] = '\0';
            if (!(p = strtok(NULL, "|"))) continue;
            strncpy(lg->type, p, sizeof lg->type - 1);
            lg->type[sizeof lg->type - 1] = '\0';
            if (!(p = strtok(NULL, "|"))) continue;
            strncpy(lg->item, p, NAME_LEN - 1);
            lg->item[NAME_LEN - 1] = '\0';
            if (!(p = strtok(NULL, "|"))) continue;
            lg->previous = atoi(p);
            if (!(p = strtok(NULL, "|"))) continue;
            lg->change = atoi(p);
            if (!(p = strtok(NULL, "|\n"))) continue;
            lg->current = atoi(p);
            log_count++;
        }
        fclose(f);
    }
}

/* ---------- Activity log & notifications ---------- */

static void add_log(const char *type, const char *item,
                    int prev, int change, int cur)
{
    LogEntry *lg;

    if (log_count >= MAX_LOGS) {            /* drop the oldest entry */
        memmove(&logs[0], &logs[1], sizeof(LogEntry) * (MAX_LOGS - 1));
        log_count = MAX_LOGS - 1;
    }
    lg = &logs[log_count++];
    now_str(lg->time, sizeof lg->time);
    snprintf(lg->type, sizeof lg->type, "%s", type);
    snprintf(lg->item, sizeof lg->item, "%s", item);
    lg->previous = prev;
    lg->change   = change;
    lg->current  = cur;
}

/* Store the message and show it immediately as a pop-up style box */
static void push_notification(const char *text)
{
    Notification *n;

    if (notif_count >= MAX_NOTIFS) {
        memmove(&notifs[0], &notifs[1], sizeof(Notification) * (MAX_NOTIFS - 1));
        notif_count = MAX_NOTIFS - 1;
    }
    n = &notifs[notif_count++];
    now_str(n->time, sizeof n->time);
    snprintf(n->text, sizeof n->text, "%s", text);

    printf("\n+------------------------------------------------+\n");
    printf("%s", text);
    printf("+------------------------------------------------+\n");
}

static void notify_restocked(const Item *it, int prev, int added)
{
    char msg[NOTIF_LEN];
    snprintf(msg, sizeof msg,
        "  [GREEN]  Stock Restocked\n"
        "  Item [%s] has been updated.\n"
        "  Previous: %d units\n"
        "  Added: +%d units\n"
        "  Current Total: %d units\n",
        it->name, prev, added, it->qty);
    push_notification(msg);
}

static void notify_reduced(const Item *it, int prev, int deducted)
{
    char msg[NOTIF_LEN];
    snprintf(msg, sizeof msg,
        "  [YELLOW]  Stock Reduced\n"
        "  Item [%s] has been issued/sold.\n"
        "  Previous: %d units\n"
        "  Deducted: -%d units\n"
        "  Current Total: %d units\n",
        it->name, prev, deducted, it->qty);
    push_notification(msg);
}

static void notify_low(const Item *it)
{
    char msg[NOTIF_LEN];
    snprintf(msg, sizeof msg,
        "  [ORANGE]  Low Stock Alert\n"
        "  Item [%s] is running low!\n"
        "  Remaining: %d units (Below threshold of %d)\n"
        "  Action: Please reorder soon.\n",
        it->name, it->qty, it->threshold);
    push_notification(msg);
}

static void notify_out(const Item *it)
{
    char msg[NOTIF_LEN];
    snprintf(msg, sizeof msg,
        "  [RED]  Out of Stock Alert\n"
        "  Item [%s] is currently OUT OF STOCK!\n"
        "  Remaining: 0 units\n"
        "  Action: Immediate reorder required.\n",
        it->name);
    push_notification(msg);
}

/* Called after stock is reduced */
static void check_stock_level(const Item *it)
{
    if (it->qty == 0)
        notify_out(it);
    else if (it->qty < it->threshold)
        notify_low(it);
}

/* ---------- Item helpers ---------- */

static Item *find_item(int id)
{
    int i;
    for (i = 0; i < item_count; i++)
        if (items[i].id == id) return &items[i];
    return NULL;
}

static const char *status_label(const Item *it)
{
    if (it->qty == 0)               return "OUT";
    if (it->qty < it->threshold)    return "LOW";
    return "OK";
}

static void print_item_table(void)
{
    int i;
    printf("\n%-5s %-26s %8s %10s %12s  %s\n",
           "ID", "Item Name", "Qty", "Threshold", "Unit Price", "Status");
    printf("---------------------------------------------------------------------\n");
    if (item_count == 0) printf("(no items)\n");
    for (i = 0; i < item_count; i++)
        printf("%-5d %-26.26s %8d %10d %12.2f  %s\n",
               items[i].id, items[i].name, items[i].qty,
               items[i].threshold, items[i].price, status_label(&items[i]));
}

/* ---------- Screens ---------- */

static void screen_dashboard(void)
{
    int i, total_units = 0, low = 0, out = 0, start;
    double value = 0;

    for (i = 0; i < item_count; i++) {
        total_units += items[i].qty;
        value       += items[i].qty * items[i].price;
        if (items[i].qty == 0)                          out++;
        else if (items[i].qty < items[i].threshold)     low++;
    }

    printf("\n================ DASHBOARD ================\n");
    printf("Inventory Overview\n");
    printf("  Total Items (SKUs) : %d\n", item_count);
    printf("  Total Units        : %d\n", total_units);
    printf("  Total Stock Value  : %.2f\n", value);
    printf("  Low Stock Items    : %d\n", low);
    printf("  Out of Stock Items : %d\n", out);

    printf("\nRecent Activities (last 5)\n");
    if (log_count == 0) printf("  (no activity yet)\n");
    start = log_count > 5 ? log_count - 5 : 0;
    for (i = log_count - 1; i >= start; i--)
        printf("  %s | %-9s | %-20.20s | %+d -> %d\n",
               logs[i].time, logs[i].type, logs[i].item,
               logs[i].change, logs[i].current);
    pause_screen();
}

static void add_new_item(void)
{
    char name[NAME_LEN];
    int qty, thr, i;
    double price;
    Item *it;

    if (item_count >= MAX_ITEMS) {
        printf("Inventory is full.\n");
        return;
    }
    read_line("Item name: ", name, sizeof name);
    if (name[0] == '\0') { printf("Name cannot be empty.\n"); return; }
    for (i = 0; name[i]; i++) if (name[i] == '|') name[i] = '-';

    if (!read_int("Initial quantity: ", 0, 1000000, &qty))   { printf("Invalid quantity.\n"); return; }
    if (!read_double("Unit price: ", &price))                { printf("Invalid price.\n");    return; }
    printf("Reorder threshold [default %d]: ", DEFAULT_THRESHOLD);
    {
        char buf[32], *end; long v;
        if (!fgets(buf, sizeof buf, stdin)) return;
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') thr = DEFAULT_THRESHOLD;
        else {
            v = strtol(buf, &end, 10);
            if (*end != '\0' || v < 0) { printf("Invalid threshold.\n"); return; }
            thr = (int)v;
        }
    }

    it = &items[item_count++];
    it->id = next_id++;
    snprintf(it->name, NAME_LEN, "%s", name);
    it->qty = qty;
    it->threshold = thr;
    it->price = price;

    add_log("NEW ITEM", it->name, 0, qty, qty);
    save_all();
    printf("Item added with ID %d.\n", it->id);
}

static void screen_inventory_list(void)
{
    int choice;
    for (;;) {
        printf("\n============== INVENTORY LIST ==============");
        print_item_table();
        printf("\n  1. Add New Item\n  0. Back\n");
        if (!read_int("Select: ", 0, 1, &choice)) { printf("Invalid choice.\n"); continue; }
        if (choice == 0) return;
        add_new_item();
    }
}

static void screen_stock_in(void)
{
    int id, qty, prev;
    Item *it;

    printf("\n================== STOCK IN ==================");
    print_item_table();
    if (!read_int("\nItem ID: ", 1, 1000000, &id) || !(it = find_item(id))) {
        printf("Item not found.\n"); pause_screen(); return;
    }
    if (!read_int("Quantity to add: ", 1, 1000000, &qty)) {
        printf("Invalid quantity.\n"); pause_screen(); return;
    }

    prev = it->qty;
    it->qty += qty;
    add_log("STOCK IN", it->name, prev, qty, it->qty);
    notify_restocked(it, prev, qty);
    save_all();
    pause_screen();
}

static void screen_stock_out(void)
{
    int id, qty, prev;
    Item *it;

    printf("\n================== STOCK OUT =================");
    print_item_table();
    if (!read_int("\nItem ID: ", 1, 1000000, &id) || !(it = find_item(id))) {
        printf("Item not found.\n"); pause_screen(); return;
    }
    if (!read_int("Quantity to issue: ", 1, 1000000, &qty)) {
        printf("Invalid quantity.\n"); pause_screen(); return;
    }
    if (qty > it->qty) {
        printf("Error: only %d units available for [%s].\n", it->qty, it->name);
        pause_screen(); return;
    }

    prev = it->qty;
    it->qty -= qty;
    add_log("STOCK OUT", it->name, prev, -qty, it->qty);
    notify_reduced(it, prev, qty);
    check_stock_level(it);
    save_all();
    pause_screen();
}

static void screen_alerts(void)
{
    int i, choice;

    printf("\n========== ALERTS & NOTIFICATIONS ==========\n");

    /* Current stock problems */
    printf("\nCurrent Stock Warnings:\n");
    {
        int found = 0;
        for (i = 0; i < item_count; i++) {
            if (items[i].qty == 0) {
                printf("  [RED]    %-26.26s OUT OF STOCK - reorder immediately\n", items[i].name);
                found = 1;
            } else if (items[i].qty < items[i].threshold) {
                printf("  [ORANGE] %-26.26s %d units (threshold %d) - reorder soon\n",
                       items[i].name, items[i].qty, items[i].threshold);
                found = 1;
            }
        }
        if (!found) printf("  All items are above their thresholds.\n");
    }

    /* Notification history (this session) */
    printf("\nNotification History (this session, newest first):\n");
    if (notif_count == 0) printf("  (none)\n");
    for (i = notif_count - 1; i >= 0; i--) {
        printf("\n  <%s>\n%s", notifs[i].time, notifs[i].text);
    }

    printf("\n  1. Clear Notifications\n  0. Back\n");
    if (read_int("Select: ", 0, 1, &choice) && choice == 1) {
        notif_count = 0;
        printf("Notifications cleared.\n");
    }
}

static void screen_thresholds(void)
{
    int id, thr;
    Item *it;

    printf("\n============== REORDER THRESHOLDS ==============");
    print_item_table();
    if (!read_int("\nItem ID (0 = back): ", 0, 1000000, &id)) {
        printf("Invalid ID.\n"); pause_screen(); return;
    }
    if (id == 0) return;
    if (!(it = find_item(id))) { printf("Item not found.\n"); pause_screen(); return; }

    printf("Current threshold for [%s]: %d\n", it->name, it->threshold);
    if (!read_int("New minimum stock limit: ", 0, 1000000, &thr)) {
        printf("Invalid value.\n"); pause_screen(); return;
    }
    add_log("THRESHOLD", it->name, it->threshold, thr - it->threshold, thr);
    it->threshold = thr;
    save_all();
    printf("Threshold updated to %d units.\n", thr);
    if (it->qty < it->threshold && it->qty > 0) notify_low(it);
    pause_screen();
}

static void screen_activity_log(void)
{
    int i;
    printf("\n================= ACTIVITY LOG =================\n");
    printf("%-19s  %-9s  %-22s %9s %9s %9s\n",
           "Time", "Type", "Item", "Previous", "Change", "Current");
    printf("-----------------------------------------------------------------------------------\n");
    if (log_count == 0) printf("(no activity yet)\n");
    for (i = log_count - 1; i >= 0; i--)
        printf("%-19s  %-9s  %-22.22s %9d %+9d %9d\n",
               logs[i].time, logs[i].type, logs[i].item,
               logs[i].previous, logs[i].change, logs[i].current);
    pause_screen();
}

/* ---------- Main ---------- */

int main(void)
{
    int choice;

    load_all();

    for (;;) {
        printf("\n=====================================\n");
        printf("   INVENTORY MANAGEMENT SYSTEM\n");
        printf("=====================================\n");
        printf("  1. Dashboard\n");
        printf("  2. Inventory List\n");
        printf("  3. Stock In\n");
        printf("  4. Stock Out\n");
        printf("  5. Alerts & Notifications\n");
        printf("  6. Reorder Thresholds\n");
        printf("  7. Activity Log\n");
        printf("  0. Exit\n");

        if (!read_int("Select: ", 0, 7, &choice)) {
            if (feof(stdin)) break;
            printf("Invalid choice.\n");
            continue;
        }
        switch (choice) {
            case 1: screen_dashboard();      break;
            case 2: screen_inventory_list(); break;
            case 3: screen_stock_in();       break;
            case 4: screen_stock_out();      break;
            case 5: screen_alerts();         break;
            case 6: screen_thresholds();     break;
            case 7: screen_activity_log();   break;
            case 0: save_all(); printf("Goodbye!\n"); return 0;
        }
    }
    save_all();
    return 0;
}
