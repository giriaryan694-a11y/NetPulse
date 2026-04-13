/*
 * ╔══════════════════════════════════════════════════╗
 * ║           NetPulse - Network Scanner             ║
 * ║       Rootless ICMP Discovery Tool v1.0          ║
 * ║              Made by Aryan Giri                  ║
 * ╚══════════════════════════════════════════════════╝
 *
 * Description : Parallel ICMP-based host discovery for Termux
 *               Works without root using system ping binary.
 * Platform    : Android/Termux (Linux ARMv8/x86)
 * Compile     : gcc netpulse.c -o netpulse -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>
#include <sys/ioctl.h>  /* ioctl(), TIOCGWINSZ, struct winsize — terminal size */

/* ─── Configuration ────────────────────────────────────────────────────────── */
#define MAX_HOSTS     65534   /* Max IPs in any subnet up to /2               */
#define THREAD_POOL   200     /* Parallel ping threads                         */
#define IP_LEN        16      /* Max IPv4 string length (xxx.xxx.xxx.xxx + \0) */
#define PING_TIMEOUT  1       /* Seconds to wait per ping reply               */
#define PING_COUNT    1       /* ICMP echo requests per host                  */
#define VERSION       "1.0"

/* ─── ANSI Colors ──────────────────────────────────────────────────────────── */
#define RED       "\033[1;31m"
#define GREEN     "\033[1;32m"
#define YELLOW    "\033[1;33m"
#define BLUE      "\033[1;34m"
#define MAGENTA   "\033[1;35m"
#define CYAN      "\033[1;36m"
#define WHITE     "\033[1;37m"
#define BOLD      "\033[1m"
#define DIM       "\033[2m"
#define RESET     "\033[0m"

/* ─── Global Work Queue ────────────────────────────────────────────────────── */
static char              work_queue[MAX_HOSTS][IP_LEN];
static volatile int      work_size  = 0;
static volatile int      work_index = 0;
static pthread_mutex_t   work_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ─── Global Results ───────────────────────────────────────────────────────── */
static char              alive_hosts[MAX_HOSTS][IP_LEN];
static volatile int      alive_count = 0;
static pthread_mutex_t   result_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ─── Progress Counter ─────────────────────────────────────────────────────── */
static volatile int      pinged = 0;
static pthread_mutex_t   prog_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ─── Scan Meta (for CSV) ──────────────────────────────────────────────────── */
static char  g_gateway[IP_LEN];
static char  g_mask[IP_LEN];
static int   g_prefix = 0;
static char  g_network[IP_LEN];


/* ══════════════════════════════════════════════════════════════════════════════
 * TERMINAL WIDTH DETECTION
 *   Priority: ioctl(TIOCGWINSZ) → $COLUMNS env var → fallback 80
 * ══════════════════════════════════════════════════════════════════════════════ */
static int get_term_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col >= 10)
        return (int)ws.ws_col;
    const char *env_cols = getenv("COLUMNS");
    if (env_cols) {
        int c = atoi(env_cols);
        if (c > 0) return c;
    }
    return 80; /* safe default */
}

/* ── Helper: print a box horizontal rule (all UTF-8 single-width chars) ──
 *   e.g. box_hrule("╔", "═", "╗", 38) → ╔══...══╗\n
 *   'n' = number of fill repetitions (= inner width of box)              */
static void box_hrule(const char *left, const char *fill,
                       const char *right, int n) {
    printf(CYAN "%s", left);
    for (int i = 0; i < n; i++) printf("%s", fill);
    printf("%s\n" RESET, right);
}

/* ── Helper: print a plain-text string centered inside a box row ──────────
 *   box_w  = total box width including the two border characters
 *   text   = plain string (no embedded ANSI codes)
 *   e.g.  ║        NetPulse         ║                                     */
static void box_row(const char *text, int box_w) {
    int inner    = box_w - 2;           /* usable cols between │ borders    */
    int text_len = (int)strlen(text);
    int pad      = inner - text_len;
    if (pad < 0) pad = 0;
    int lpad = pad / 2;
    int rpad = pad - lpad;
    printf(CYAN "║" RESET "%*s%s%*s" CYAN "║\n" RESET,
           lpad, "", text, rpad, "");
}

/* ══════════════════════════════════════════════════════════════════════════════
 * ADAPTIVE BANNER — auto-detects terminal width and picks the right tier
 *
 *  ≥ 75 cols  →  WIDE   : Full 8-letter ASCII block-art (70 cols) + tagline box
 *  44–74 cols →  MEDIUM : Compact Unicode box, stylised N·E·T·P·U·L·S·E title
 *  < 44 cols  →  NARROW : Minimal 3-line text-only banner
 * ══════════════════════════════════════════════════════════════════════════════ */
static void print_banner(void) {
    int cols = get_term_width();
    printf("\n");

    /* ════════════════════════════════════════════════════════
     * TIER 1 — WIDE (≥ 75 cols)
     *   Full block-letter logo (70 cols) + tagline box below
     * ════════════════════════════════════════════════════════ */
    if (cols >= 75) {
        /* Block-letter logo — each row is exactly 70 cols (2 indent + 68 art) */
        printf(CYAN
            "  ███╗   ██╗███████╗████████╗██████╗ ██╗   ██╗██╗     ███████╗███████╗\n"
            "  ████╗  ██║██╔════╝╚══██╔══╝██╔══██╗██║   ██║██║     ██╔════╝██╔════╝\n"
            "  ██╔██╗ ██║█████╗     ██║   ██████╔╝██║   ██║██║     ███████╗█████╗  \n"
            "  ██║╚██╗██║██╔══╝     ██║   ██╔═══╝ ██║   ██║██║     ╚════██║██╔══╝  \n"
            "  ██║ ╚████║███████╗   ██║   ██║     ╚██████╔╝███████╗███████║███████╗\n"
            "  ╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═╝      ╚═════╝ ╚══════╝╚══════╝╚══════╝\n"
            RESET);

        /*
         * Tagline box: aligns with the art above (2-space indent).
         * Inner width = 68 (matches art width of 68 cols).
         * Content string is left-padded to fill exactly 68 chars.
         */
        #define WIDE_INNER 68
        char tag[WIDE_INNER + 1];
        snprintf(tag, sizeof(tag),
                 "  Rootless ICMP Scanner  |  v%-4s  |  Made by Aryan Giri",
                 VERSION);
        /* Pad to exactly WIDE_INNER with spaces */
        int tag_len = (int)strlen(tag);
        int tag_rpad = WIDE_INNER - tag_len;
        if (tag_rpad < 0) tag_rpad = 0;

        printf(DIM CYAN "  ┌");
        for (int i = 0; i < WIDE_INNER; i++) printf("─");
        printf("┐\n");
        printf("  │" RESET "%s%*s" DIM CYAN "│\n", tag, tag_rpad, "");
        printf("  └");
        for (int i = 0; i < WIDE_INNER; i++) printf("─");
        printf("┘\n" RESET "\n");
        #undef WIDE_INNER

    /* ════════════════════════════════════════════════════════
     * TIER 2 — MEDIUM (44–74 cols)
     *   Full-width Unicode box, spaced-letter title, tagline
     * ════════════════════════════════════════════════════════ */
    } else if (cols >= 44) {
        /*
         * Box spans the full terminal width.
         * box_w  = total cols (border-to-border)
         * inner  = box_w - 2  (space between the two ║ chars)
         */
        int box_w = cols;
        int inner = box_w - 2;

        /* ── Top border ── */
        box_hrule("╔", "═", "╗", inner);

        /* ── Empty padding row ── */
        box_row("", box_w);

        /* ── Spaced-letter title (scales naturally) ── */
        box_row("◆  N E T P U L S E  ◆", box_w);

        /* ── Subtitle rows ── */
        box_row("", box_w);
        box_row("Rootless ICMP Network Scanner", box_w);
        box_row("Made by Aryan Giri", box_w);
        box_row("", box_w);

        /* ── Mid-divider ── */
        box_hrule("╠", "═", "╣", inner);

        /* ── Version / info row ── */
        char ver[64];
        snprintf(ver, sizeof(ver), "v%s  |  ICMP Discovery  |  Termux", VERSION);
        box_row(ver, box_w);

        /* ── Bottom border ── */
        box_hrule("╚", "═", "╝", inner);
        printf("\n");

    /* ════════════════════════════════════════════════════════
     * TIER 3 — NARROW (< 44 cols)
     *   Minimal 3-line plain-text banner; no box drawing.
     *   Centres what it can within available width.
     * ════════════════════════════════════════════════════════ */
    } else {
        /* Simple centred lines — use printf padding tricks */
        int pad = (cols - 18) / 2; /* "[ NetPulse v1.0 ]" = 18 chars */
        if (pad < 0) pad = 0;
        printf(CYAN "%*s[ NetPulse v%-4s]\n" RESET, pad, "", VERSION);

        pad = (cols - 26) / 2;     /* "Rootless ICMP Scanner" */
        if (pad < 0) pad = 0;
        printf(DIM "%*sRootless ICMP Scanner\n" RESET, pad, "");

        pad = (cols - 18) / 2;     /* "Made by Aryan Giri" */
        if (pad < 0) pad = 0;
        printf(DIM "%*sMade by Aryan Giri\n\n" RESET, pad, "");
    }
}

/* ══════════════════════════════════════════════════════════════════════════════
 * UTILITY — Subnet mask string → CIDR prefix length
 *   e.g. "255.255.255.0" → 24
 * ══════════════════════════════════════════════════════════════════════════════ */
static int mask_to_prefix(const char *mask_str) {
    struct in_addr addr;
    if (inet_aton(mask_str, &addr) == 0) return -1;

    uint32_t mask = ntohl(addr.s_addr);
    int prefix = 0;

    /* Count leading 1-bits */
    while (mask & 0x80000000U) {
        prefix++;
        mask <<= 1;
    }
    /* Validate: remaining bits must all be 0 (contiguous mask) */
    if (mask != 0) return -1;

    return prefix;
}

/* ══════════════════════════════════════════════════════════════════════════════
 * UTILITY — Validate IPv4 address string
 * ══════════════════════════════════════════════════════════════════════════════ */
static int valid_ip(const char *ip_str) {
    struct in_addr addr;
    return (inet_aton(ip_str, &addr) != 0);
}

/* ══════════════════════════════════════════════════════════════════════════════
 * GENERATE IP RANGE
 *   From gateway + subnet mask → fill work_queue[] with all host IPs
 *   Returns: number of IPs generated, or -1 on error
 * ══════════════════════════════════════════════════════════════════════════════ */
static int generate_ip_range(void) {
    struct in_addr gw_addr, mask_addr;

    if (inet_aton(g_gateway, &gw_addr) == 0 ||
        inet_aton(g_mask,    &mask_addr) == 0) {
        return -1;
    }

    uint32_t gw        = ntohl(gw_addr.s_addr);
    uint32_t mask      = ntohl(mask_addr.s_addr);
    uint32_t network   = gw & mask;
    uint32_t broadcast = network | (~mask);

    /* Store network address string for CSV header */
    struct in_addr net_addr;
    net_addr.s_addr = htonl(network);
    strncpy(g_network, inet_ntoa(net_addr), IP_LEN - 1);

    work_size = 0;
    /* Iterate host IPs: network+1 .. broadcast-1 */
    for (uint32_t ip = network + 1; ip < broadcast && work_size < MAX_HOSTS; ip++) {
        struct in_addr tmp;
        tmp.s_addr = htonl(ip);
        strncpy(work_queue[work_size], inet_ntoa(tmp), IP_LEN - 1);
        work_queue[work_size][IP_LEN - 1] = '\0';
        work_size++;
    }

    return work_size;
}

/* ══════════════════════════════════════════════════════════════════════════════
 * PING HOST — Rootless via system ping binary
 *   Returns 1 if alive, 0 if unreachable
 * ══════════════════════════════════════════════════════════════════════════════ */
static int ping_host(const char *ip) {
    char cmd[96];
    /*
     * -c 1  : send 1 ICMP echo request
     * -W 1  : wait 1 second for reply  (Linux/Android flag)
     * -q    : quiet, suppress per-packet output
     * Redirect stdout+stderr to /dev/null; use exit-code only.
     * Exit 0 = at least 1 reply received → host is UP
     */
    snprintf(cmd, sizeof(cmd),
             "ping -c %d -W %d -q %s > /dev/null 2>&1",
             PING_COUNT, PING_TIMEOUT, ip);
    return (system(cmd) == 0) ? 1 : 0;
}

/* ══════════════════════════════════════════════════════════════════════════════
 * WORKER THREAD
 *   Continuously pulls IPs from the shared work queue and pings them.
 * ══════════════════════════════════════════════════════════════════════════════ */
static void *worker(void *arg) {
    (void)arg; /* unused */

    while (1) {
        /* ── Claim next IP from queue ── */
        pthread_mutex_lock(&work_mutex);
        if (work_index >= work_size) {
            pthread_mutex_unlock(&work_mutex);
            break; /* Queue exhausted → exit thread */
        }
        char ip[IP_LEN];
        strncpy(ip, work_queue[work_index++], IP_LEN - 1);
        ip[IP_LEN - 1] = '\0';
        pthread_mutex_unlock(&work_mutex);

        /* ── Ping ── */
        int is_up = ping_host(ip);

        /* ── Update progress bar ── */
        pthread_mutex_lock(&prog_mutex);
        pinged++;
        int local_pinged = pinged;
        pthread_mutex_unlock(&prog_mutex);

        /* Print progress every 5 hosts or when a live host is found */
        if (local_pinged % 5 == 0 || is_up) {
            printf("\r" YELLOW "  [~] Scanning: " WHITE "%d" YELLOW "/" WHITE "%d"
                   YELLOW "  │  Alive: " GREEN "%d" RESET "          ",
                   local_pinged, work_size, alive_count);
            fflush(stdout);
        }

        /* ── Record alive host ── */
        if (is_up) {
            pthread_mutex_lock(&result_mutex);
            if (alive_count < MAX_HOSTS) {
                snprintf(alive_hosts[alive_count++], IP_LEN, "%s", ip);
            }
            pthread_mutex_unlock(&result_mutex);

            /* Print discovery immediately (on new line, preserving progress) */
            printf("\r  " GREEN "✔  %-15s  " DIM "— Host is UP" RESET "\n", ip);
            fflush(stdout);
        }
    }

    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════════
 * SORT HELPERS — Compare IPv4 addresses numerically for qsort
 * ══════════════════════════════════════════════════════════════════════════════ */
static int cmp_ip(const void *a, const void *b) {
    struct in_addr ia, ib;
    inet_aton((const char *)a, &ia);
    inet_aton((const char *)b, &ib);
    uint32_t ua = ntohl(ia.s_addr);
    uint32_t ub = ntohl(ib.s_addr);
    if (ua < ub) return -1;
    if (ua > ub) return  1;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════════
 * SAVE CSV
 *   Writes results to a timestamped CSV file.
 * ══════════════════════════════════════════════════════════════════════════════ */
static void save_csv(void) {
    char filename[64];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);

    strftime(filename, sizeof(filename), "netpulse_%Y%m%d_%H%M%S.csv", tm_info);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf(RED "\n  [-] ERROR: Cannot create file '%s'\n" RESET, filename);
        return;
    }

    /* CSV header */
    fprintf(fp, "No,IP Address,Status,Gateway,Subnet Mask,CIDR,Network,Scanned At\n");

    /* Timestamp string for each row */
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

    for (int i = 0; i < alive_count; i++) {
        fprintf(fp, "%d,%s,UP,%s,%s,/%d,%s,%s\n",
                i + 1,
                alive_hosts[i],
                g_gateway,
                g_mask,
                g_prefix,
                g_network,
                ts);
    }

    fclose(fp);
    printf(GREEN "\n  [+] Results saved → " WHITE "%s" RESET "\n", filename);
}

/* ══════════════════════════════════════════════════════════════════════════════
 * PRINT RESULTS TABLE
 * ══════════════════════════════════════════════════════════════════════════════ */
static void print_results(void) {
    printf("\n\n" CYAN
        "  ┌────────────────────────────────────────────────┐\n"
        "  │               SCAN RESULTS                     │\n"
        "  ├────────┬───────────────────┬───────────────────┤\n"
        "  │  No.   │    IP Address     │      Status       │\n"
        "  ├────────┼───────────────────┼───────────────────┤\n"
        RESET);

    if (alive_count == 0) {
        printf(YELLOW "  │  " RED "  No live hosts found in this subnet.       " YELLOW "  │\n" RESET);
    } else {
        for (int i = 0; i < alive_count; i++) {
            printf(CYAN "  │ " WHITE " %-6d " CYAN "│ " GREEN " %-17s " CYAN "│ " GREEN " %-17s " CYAN "│\n" RESET,
                   i + 1, alive_hosts[i], "UP ✔");
        }
    }

    printf(CYAN
        "  └────────┴───────────────────┴───────────────────┘\n"
        RESET);

    printf(YELLOW "\n  [*] Total alive  : " WHITE "%d\n" RESET, alive_count);
    printf(YELLOW   "  [*] Total scanned: " WHITE "%d\n" RESET, work_size);
}

/* ══════════════════════════════════════════════════════════════════════════════
 * PRINT SCAN SUMMARY (before scan starts)
 * ══════════════════════════════════════════════════════════════════════════════ */
static void print_scan_info(void) {
    printf(CYAN
        "  ┌──────────────────────────────────────────────┐\n"
        "  │              SCAN CONFIGURATION              │\n"
        "  ├──────────────────────┬───────────────────────┤\n"
        RESET);
    printf(CYAN "  │ " YELLOW " %-20s " CYAN "│ " WHITE " %-21s " CYAN "│\n" RESET,
           "Gateway IP",      g_gateway);
    printf(CYAN "  │ " YELLOW " %-20s " CYAN "│ " WHITE " %-21s " CYAN "│\n" RESET,
           "Subnet Mask",     g_mask);
    printf(CYAN "  │ " YELLOW " %-20s " CYAN "│ " WHITE " /%-20d " CYAN "│\n" RESET,
           "CIDR Notation",   g_prefix);
    printf(CYAN "  │ " YELLOW " %-20s " CYAN "│ " WHITE " %-21s " CYAN "│\n" RESET,
           "Network Address", g_network);
    printf(CYAN "  │ " YELLOW " %-20s " CYAN "│ " WHITE " %-21d " CYAN "│\n" RESET,
           "Host Range Size", work_size);
    printf(CYAN "  │ " YELLOW " %-20s " CYAN "│ " WHITE " %-21d " CYAN "│\n" RESET,
           "Threads",         THREAD_POOL);
    printf(CYAN
        "  └──────────────────────┴───────────────────────┘\n\n"
        RESET);
}

/* ══════════════════════════════════════════════════════════════════════════════
 * MAIN
 * ══════════════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Disable buffering so progress prints appear immediately */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ── Banner ── */
    print_banner();

    /* ── Input: Gateway IP ── */
    printf(CYAN "  Enter Gateway IP   : " WHITE);
    if (scanf("%15s", g_gateway) != 1) {
        printf(RED "  [-] Invalid input.\n" RESET);
        return 1;
    }
    if (!valid_ip(g_gateway)) {
        printf(RED "  [-] Invalid gateway IP address: %s\n" RESET, g_gateway);
        return 1;
    }

    /* ── Input: Subnet Mask ── */
    printf(CYAN "  Enter Subnet Mask  : " WHITE);
    if (scanf("%15s", g_mask) != 1) {
        printf(RED "  [-] Invalid input.\n" RESET);
        return 1;
    }
    printf(RESET "\n");

    /* ── Convert mask → CIDR prefix ── */
    g_prefix = mask_to_prefix(g_mask);
    if (g_prefix < 0) {
        printf(RED "  [-] Invalid or non-contiguous subnet mask: %s\n" RESET, g_mask);
        printf(DIM "      Examples: 255.255.255.0 (/24)  255.255.0.0 (/16)\n" RESET);
        return 1;
    }
    if (g_prefix < 8) {
        printf(YELLOW "  [!] Warning: /%d is a very large subnet (%u hosts).\n"
               "      This scan may take a very long time.\n" RESET,
               g_prefix, (1u << (32 - g_prefix)) - 2);
        printf(YELLOW "  Continue? (y/n): " WHITE);
        char ch = 'n';
        int r = scanf(" %c", &ch); (void)r;
        printf(RESET);
        if (ch != 'y' && ch != 'Y') {
            printf(DIM "  Aborted.\n" RESET);
            return 0;
        }
    }

    /* ── Generate IP range ── */
    int count = generate_ip_range();
    if (count <= 0) {
        printf(RED "  [-] Failed to generate IP range. Check your inputs.\n" RESET);
        return 1;
    }

    /* ── Print scan config ── */
    print_scan_info();

    /* ── Start timer ── */
    time_t start_time = time(NULL);

    /* ── Launch thread pool ── */
    printf(YELLOW "  [*] Starting scan...\n\n" RESET);

    int actual_threads = (THREAD_POOL < work_size) ? THREAD_POOL : work_size;
    pthread_t threads[THREAD_POOL];

    for (int i = 0; i < actual_threads; i++) {
        if (pthread_create(&threads[i], NULL, worker, NULL) != 0) {
            printf(RED "  [-] Failed to create thread %d\n" RESET, i);
        }
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < actual_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* ── Scan complete ── */
    time_t elapsed = time(NULL) - start_time;

    /* Clear progress line */
    printf("\r%-70s\r", "");

    /* Sort results by IP numerically */
    qsort(alive_hosts, alive_count, IP_LEN, cmp_ip);

    /* ── Print results table ── */
    print_results();
    printf(DIM "  [*] Scan duration : %ld second(s)\n" RESET, elapsed);

    /* ── Ask to save ── */
    char save_choice = 'n';
    printf(YELLOW "\n  [?] Save results to CSV? (y/n): " WHITE);
    if (scanf(" %c", &save_choice) != 1) save_choice = 'n';
    printf(RESET);

    if (save_choice == 'y' || save_choice == 'Y') {
        save_csv();
    } else {
        printf(DIM "\n  [-] Skipping save.\n" RESET);
    }

    /* ── Footer ── */
    printf(CYAN "\n  ✦ NetPulse v" VERSION " | Made by Aryan Giri\n\n" RESET);

    return 0;
}
