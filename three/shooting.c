/* shooting.c - RS232C dual-port real-time shooting duel (multitask) */
#include "mtk_c.h"

/* I/O from csys68k.c */
extern void outbyte(int port, unsigned char c);
extern char inbyte(int port);

#define PORT_P1 0
#define PORT_P2 1

#define BOARD_W 40
#define BOARD_H 8
#define MAX_BULLETS 5
#define COOLDOWN_TICKS 6
#define BULLET_STEP_DIV 6

typedef struct {
    int x;
    int y;
    int alive;
    int owner;
} BULLET;

typedef struct {
    int p1_y;
    int p2_y;
    int p1_score;
    int p2_score;
    int p1_cooldown;
    int p2_cooldown;
    int input_dir[2];
    int input_fire[2];
    char last_key[2];
    unsigned int rng;
    unsigned int tick;
    BULLET bullets[2][MAX_BULLETS];
} GAMESTATE;

static GAMESTATE g;

static void out_str(int port, const char *s) {
    while (*s) {
        outbyte(port, (unsigned char)*s++);
    }
}

static void out_num(int port, int value) {
    char buf[12];
    int i = 0;
    if (value == 0) {
        outbyte(port, '0');
        return;
    }
    if (value < 0) {
        outbyte(port, '-');
        value = -value;
    }
    while (value > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i-- > 0) {
        outbyte(port, (unsigned char)buf[i]);
    }
}

static unsigned int rng_next(void) {
    g.rng = g.rng * 1103515245u + 12345u + g.tick;
    return g.rng;
}

static void clear_bullets(void) {
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < MAX_BULLETS; i++) {
            g.bullets[p][i].alive = 0;
        }
    }
}

static void reset_round(void) {
    g.p1_cooldown = 0;
    g.p2_cooldown = 0;
    clear_bullets();
}

static void init_game(void) {
    int span = BOARD_H - 2;
    g.rng = 1234567u;
    g.tick = 0;
    g.p1_score = 0;
    g.p2_score = 0;
    g.p1_y = 1 + (int)(rng_next() % span);
    g.p2_y = 1 + (int)(rng_next() % span);
    g.input_dir[0] = 0;
    g.input_dir[1] = 0;
    g.input_fire[0] = 0;
    g.input_fire[1] = 0;
    g.last_key[0] = '-';
    g.last_key[1] = '-';
    reset_round();
}

static void spawn_bullet(int owner) {
    int p1_x = 2;
    int p2_x = BOARD_W - 3;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g.bullets[owner][i].alive) {
            g.bullets[owner][i].alive = 1;
            g.bullets[owner][i].owner = owner;
            g.bullets[owner][i].y = (owner == 0) ? g.p1_y : g.p2_y;
            g.bullets[owner][i].x = (owner == 0) ? (p1_x + 1) : (p2_x - 1);
            break;
        }
    }
}

static int clamp_y(int y) {
    if (y < 1) return 1;
    if (y > BOARD_H - 2) return BOARD_H - 2;
    return y;
}

static int step_bullets(void) {
    int p1_x = 2;
    int p2_x = BOARD_W - 3;

    if ((g.tick % BULLET_STEP_DIV) != 0) {
        return 0;
    }

    for (int owner = 0; owner < 2; owner++) {
        int dx = (owner == 0) ? 1 : -1;
        for (int i = 0; i < MAX_BULLETS; i++) {
            BULLET *b = &g.bullets[owner][i];
            if (!b->alive) continue;
            b->x += dx;
            if (b->x <= 0 || b->x >= BOARD_W - 1) {
                b->alive = 0;
                continue;
            }
            if (owner == 0 && b->x == p2_x && b->y == g.p2_y) {
                g.p1_score++;
                return 1;
            }
            if (owner == 1 && b->x == p1_x && b->y == g.p1_y) {
                g.p2_score++;
                return 1;
            }
        }
    }
    return 0;
}

static void render_port(int port, const GAMESTATE *s) {
    char line[BOARD_W + 1];
    int p1_x = 2;
    int p2_x = BOARD_W - 3;

    out_str(port, "\x1b[H\x1b[2J");
    out_str(port, "Score ");
    out_num(port, s->p1_score);
    out_str(port, " - ");
    out_num(port, s->p2_score);
    out_str(port, "\r\n");

    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            char ch = ' ';
            if (y == 0 || y == BOARD_H - 1) {
                ch = '-';
            }
            line[x] = ch;
        }
        line[BOARD_W] = '\0';

        if (y > 0 && y < BOARD_H - 1) {
            for (int owner = 0; owner < 2; owner++) {
                for (int i = 0; i < MAX_BULLETS; i++) {
                    const BULLET *b = &s->bullets[owner][i];
                    if (!b->alive || b->y != y) continue;
                    if (b->x > 0 && b->x < BOARD_W - 1) {
                        line[b->x] = (owner == 0) ? '>' : '<';
                    }
                }
            }

            if (y == s->p1_y) {
                line[p1_x] = 'A';
            }
            if (y == s->p2_y) {
                line[p2_x] = 'B';
            }
        }

        out_str(port, line);
        out_str(port, "\r\n");
    }
}

/* input task for port 0 (player 1) */
void task_input_p1(void) {
    while (1) {
        char c = inbyte(PORT_P1);
        P(0);
        g.last_key[0] = c;
        if (c == 'w' || c == 'W') g.input_dir[0] = -1;
        else if (c == 's' || c == 'S') g.input_dir[0] = 1;
        else if (c == ' ') g.input_fire[0] = 1;
        else if (c == 'i' || c == 'I') g.input_dir[1] = -1;
        else if (c == 'k' || c == 'K') g.input_dir[1] = 1;
        else if (c == 'p' || c == 'P') g.input_fire[1] = 1;
        V(0);
    }
}

/* input task for port 1 (player 2) */
void task_input_p2(void) {
    while (1) {
        char c = inbyte(PORT_P2);
        P(0);
        g.last_key[1] = c;
        if (c == 'w' || c == 'W') g.input_dir[0] = -1;
        else if (c == 's' || c == 'S') g.input_dir[0] = 1;
        else if (c == ' ') g.input_fire[0] = 1;
        else if (c == 'i' || c == 'I') g.input_dir[1] = -1;
        else if (c == 'k' || c == 'K') g.input_dir[1] = 1;
        else if (c == 'p' || c == 'P') g.input_fire[1] = 1;
        V(0);
    }
}

/* game update task */
void task_game(void) {
    init_game();
    while (1) {
        int dir1, dir2, fire1, fire2;
        P(0);
        dir1 = g.input_dir[0];
        dir2 = g.input_dir[1];
        fire1 = g.input_fire[0];
        fire2 = g.input_fire[1];
        g.input_dir[0] = 0;
        g.input_dir[1] = 0;
        g.input_fire[0] = 0;
        g.input_fire[1] = 0;

        g.p1_y = clamp_y(g.p1_y + dir1);
        g.p2_y = clamp_y(g.p2_y + dir2);

        if (g.p1_cooldown > 0) g.p1_cooldown--;
        if (g.p2_cooldown > 0) g.p2_cooldown--;

        if (fire1 && g.p1_cooldown == 0) {
            spawn_bullet(0);
            g.p1_cooldown = COOLDOWN_TICKS;
        }
        if (fire2 && g.p2_cooldown == 0) {
            spawn_bullet(1);
            g.p2_cooldown = COOLDOWN_TICKS;
        }

        if (step_bullets()) {
            reset_round();
        }

        g.tick++;
        rng_next();
        V(0);

        // for (volatile int i = 0; i < 200; i++);
    }
}

/* render task */
void task_render(void) {
    while (1) {
        GAMESTATE snapshot;
        P(0);
        snapshot = g;
        V(0);

        render_port(PORT_P1, &snapshot);
        render_port(PORT_P2, &snapshot);

        // for (volatile int i = 0; i < 400; i++);
    }
}

void exit(int status) {
    while (1);
}

int main(void) {
    init_kernel();

    semaphore[0].count = 1;
    semaphore[0].task_list = NULLTASKID;

    set_task(task_input_p1);
    set_task(task_input_p2);
    set_task(task_game);
    set_task(task_render);

    begin_sch();
    return 0;
}
