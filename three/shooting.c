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
#define BULLET_STEP_DIV 18
#define TARGET_SCORE 30
#define SEM_MUTEX 0
#define SEM_RENDER 1
#define SEM_RENDER_DONE 2

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
    int game_over;
    int winner;
    BULLET bullets[2][MAX_BULLETS];
} GAMESTATE;

/* 前回の描画状態を保持（差分描画用） */
typedef struct {
    int p1_y;
    int p2_y;
    int p1_score;
    int p2_score;
    char board[BOARD_H][BOARD_W];
    int game_over;
    int winner;
    int initialized;
} RENDER_STATE;

static GAMESTATE g;
static RENDER_STATE render_state[2]; /* PORT_P1, PORT_P2用 */

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

/* カーソル位置移動（1-based座標） */
static void cursor_move(int port, int row, int col) {
    char buf[16];
    int i = 0;
    buf[i++] = '\x1b';
    buf[i++] = '[';
    /* row */
    int r = row;
    if (r >= 100) buf[i++] = (char)('0' + (r / 100));
    if (r >= 10) buf[i++] = (char)('0' + ((r / 10) % 10));
    buf[i++] = (char)('0' + (r % 10));
    buf[i++] = ';';
    /* col */
    int c = col;
    if (c >= 100) buf[i++] = (char)('0' + (c / 100));
    if (c >= 10) buf[i++] = (char)('0' + ((c / 10) % 10));
    buf[i++] = (char)('0' + (c % 10));
    buf[i++] = 'H';
    for (int j = 0; j < i; j++) {
        outbyte(port, buf[j]);
    }
}

/* 1文字を指定位置に出力 */
static void put_char_at(int port, int row, int col, char ch) {
    cursor_move(port, row, col);
    outbyte(port, (unsigned char)ch);
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
    g.game_over = 0;
    g.winner = -1;
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

static int step_bullets(int *moved_out) {
    int p1_x = 2;
    int p2_x = BOARD_W - 3;
    int moved = 0;

    if ((g.tick % BULLET_STEP_DIV) != 0) {
        if (moved_out) *moved_out = 0;
        return 0;
    }

    for (int owner = 0; owner < 2; owner++) {
        int dx = (owner == 0) ? 1 : -1;
        for (int i = 0; i < MAX_BULLETS; i++) {
            BULLET *b = &g.bullets[owner][i];
            if (!b->alive) continue;
            b->x += dx;
            moved = 1;
            if (b->x <= 0 || b->x >= BOARD_W - 1) {
                b->alive = 0;
                continue;
            }
            if (owner == 0 && b->x == p2_x && b->y == g.p2_y) {
                g.p1_score++;
                if (moved_out) *moved_out = moved;
                return 1;
            }
            if (owner == 1 && b->x == p1_x && b->y == g.p1_y) {
                g.p2_score++;
                if (moved_out) *moved_out = moved;
                return 1;
            }
        }
    }
    if (moved_out) *moved_out = moved;
    return 0;
}

/* 現在の盤面状態を計算 */
static void build_board(char board[BOARD_H][BOARD_W], const GAMESTATE *s) {
    int p1_x = 2;
    int p2_x = BOARD_W - 3;
    
    /* 盤面を初期化 */
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            if (y == 0 || y == BOARD_H - 1) {
                board[y][x] = '-';
            } else {
                board[y][x] = ' ';
            }
        }
    }
    
    /* プレイヤーを配置 */
    board[s->p1_y][p1_x] = 'A';
    board[s->p2_y][p2_x] = 'B';
    
    /* 弾を配置 */
    for (int owner = 0; owner < 2; owner++) {
        for (int i = 0; i < MAX_BULLETS; i++) {
            const BULLET *b = &s->bullets[owner][i];
            if (b->alive && b->x > 0 && b->x < BOARD_W - 1 && b->y > 0 && b->y < BOARD_H - 1) {
                board[b->y][b->x] = (owner == 0) ? '>' : '<';
            }
        }
    }
}

static void render_port(int port, const GAMESTATE *s) {
    RENDER_STATE *prev = &render_state[port];
    char new_board[BOARD_H][BOARD_W];
    int p1_x = 2;
    int p2_x = BOARD_W - 3;
    int need_full_clear = !prev->initialized;
    const char *msg = 0;
    
    /* 現在の盤面状態を計算 */
    build_board(new_board, s);

    /* 初回描画またはラウンドリセット時は全画面クリア */
    if (prev->game_over != s->game_over || prev->winner != s->winner) {
        need_full_clear = 1;
    }
    if (need_full_clear) {
        out_str(port, "\x1b[H\x1b[2J");
        prev->initialized = 1;
        
        /* スコア行を描画 */
        out_str(port, "Score ");
        out_num(port, s->p1_score);
        out_str(port, " - ");
        out_num(port, s->p2_score);
        out_str(port, "\r\n");
        
        /* 全盤面を描画 */
        for (int y = 0; y < BOARD_H; y++) {
            for (int x = 0; x < BOARD_W; x++) {
                outbyte(port, (unsigned char)new_board[y][x]);
            }
            out_str(port, "\r\n");
        }

        /* ゲーム終了メッセージ */
        if (s->game_over) {
            if (s->winner == 0) {
                msg = "A Winner! Press any key to restart";
            } else if (s->winner == 1) {
                msg = "B Winner! Press any key to restart";
            } else {
                msg = "Game Over! Press any key to restart";
            }
            cursor_move(port, BOARD_H + 2, 1);
            out_str(port, msg);
        }
        
        /* 前回状態を保存 */
        prev->p1_y = s->p1_y;
        prev->p2_y = s->p2_y;
        prev->p1_score = s->p1_score;
        prev->p2_score = s->p2_score;
        prev->game_over = s->game_over;
        prev->winner = s->winner;
        for (int y = 0; y < BOARD_H; y++) {
            for (int x = 0; x < BOARD_W; x++) {
                prev->board[y][x] = new_board[y][x];
            }
        }
        return;
    }
    
    /* スコアが変更された場合のみ更新 */
    if (prev->p1_score != s->p1_score || prev->p2_score != s->p2_score) {
        cursor_move(port, 1, 1);
        out_str(port, "Score ");
        out_num(port, s->p1_score);
        out_str(port, " - ");
        out_num(port, s->p2_score);
        prev->p1_score = s->p1_score;
        prev->p2_score = s->p2_score;
    }

    if (prev->game_over != s->game_over || prev->winner != s->winner) {
        if (s->game_over) {
            if (s->winner == 0) {
                msg = "A Winner! Press any key to restart";
            } else if (s->winner == 1) {
                msg = "B Winner! Press any key to restart";
            } else {
                msg = "Game Over! Press any key to restart";
            }
            cursor_move(port, BOARD_H + 2, 1);
            out_str(port, msg);
        } else {
            cursor_move(port, BOARD_H + 2, 1);
            for (int i = 0; i < BOARD_W + 10; i++) outbyte(port, ' ');
        }
        prev->game_over = s->game_over;
        prev->winner = s->winner;
    }
    
    /* プレイヤー位置の変更を検出して更新 */
    if (prev->p1_y != s->p1_y) {
        /* 前の位置を消去（prev->boardも更新） */
        put_char_at(port, prev->p1_y + 2, p1_x + 1, ' ');
        prev->board[prev->p1_y][p1_x] = ' ';
        /* 新しい位置に描画 */
        put_char_at(port, s->p1_y + 2, p1_x + 1, 'A');
        prev->board[s->p1_y][p1_x] = 'A';
        prev->p1_y = s->p1_y;
    }
    
    if (prev->p2_y != s->p2_y) {
        /* 前の位置を消去（prev->boardも更新） */
        put_char_at(port, prev->p2_y + 2, p2_x + 1, ' ');
        prev->board[prev->p2_y][p2_x] = ' ';
        /* 新しい位置に描画 */
        put_char_at(port, s->p2_y + 2, p2_x + 1, 'B');
        prev->board[s->p2_y][p2_x] = 'B';
        prev->p2_y = s->p2_y;
    }
    
    /* 弾の位置を差分更新（プレイヤー位置も含めて全セルをチェック） */
    for (int y = 1; y < BOARD_H - 1; y++) {
        for (int x = 1; x < BOARD_W - 1; x++) {
            char old_ch = prev->board[y][x];
            char new_ch = new_board[y][x];
            
            /* 変更があった場合のみ更新 */
            if (old_ch != new_ch) {
                put_char_at(port, y + 2, x + 1, new_ch);
                prev->board[y][x] = new_ch;
            }
        }
    }
}

/* input task for port 0 (player 1) */
void task_input_p1(void) {
    while (1) {
        char c = inbyte(PORT_P1);
        P(SEM_MUTEX);
        /* 共有状態への入力反映を排他で行う */
        g.last_key[0] = c;
        if (c == 'w' || c == 'W') g.input_dir[0] = -1;
        else if (c == 's' || c == 'S') g.input_dir[0] = 1;
        else if (c == ' ') g.input_fire[0] = 1;
        else if (c == 'i' || c == 'I') g.input_dir[1] = -1;
        else if (c == 'k' || c == 'K') g.input_dir[1] = 1;
        else if (c == 'p' || c == 'P') g.input_fire[1] = 1;
        V(SEM_MUTEX);
    }
}

/* input task for port 1 (player 2) */
void task_input_p2(void) {
    while (1) {
        char c = inbyte(PORT_P2);
        P(SEM_MUTEX);
        /* 入力の割り当てはport0と同じ（共有状態に集約） */
        g.last_key[1] = c;
        if (c == 'w' || c == 'W') g.input_dir[0] = -1;
        else if (c == 's' || c == 'S') g.input_dir[0] = 1;
        else if (c == ' ') g.input_fire[0] = 1;
        else if (c == 'i' || c == 'I') g.input_dir[1] = -1;
        else if (c == 'k' || c == 'K') g.input_dir[1] = 1;
        else if (c == 'p' || c == 'P') g.input_fire[1] = 1;
        V(SEM_MUTEX);
    }
}

/* game update task */
void task_game(void) {
    init_game();
    while (1) {
        int dir1, dir2, fire1, fire2;
        int changed = 0;
        static int first_render = 1;
        P(SEM_MUTEX);
        /* 入力フラグをまとめて読み出す */
        dir1 = g.input_dir[0];
        dir2 = g.input_dir[1];
        fire1 = g.input_fire[0];
        fire2 = g.input_fire[1];
        g.input_dir[0] = 0;
        g.input_dir[1] = 0;
        g.input_fire[0] = 0;
        g.input_fire[1] = 0;

        if (g.game_over) {
            if (dir1 || dir2 || fire1 || fire2) {
                init_game();
                first_render = 1;
                changed = 1;
            }
            V(SEM_MUTEX);
            if (changed) {
                V(SEM_RENDER);
                P(SEM_RENDER_DONE);
            }
            continue;
        }

        if (dir1 != 0 || dir2 != 0) {
            changed = 1;
        }
        g.p1_y = clamp_y(g.p1_y + dir1);
        g.p2_y = clamp_y(g.p2_y + dir2);

        if (g.p1_cooldown > 0) g.p1_cooldown--;
        if (g.p2_cooldown > 0) g.p2_cooldown--;

        if (fire1 && g.p1_cooldown == 0) {
            spawn_bullet(0);
            g.p1_cooldown = COOLDOWN_TICKS;
            changed = 1;
        }
        if (fire2 && g.p2_cooldown == 0) {
            spawn_bullet(1);
            g.p2_cooldown = COOLDOWN_TICKS;
            changed = 1;
        }

        {
            int moved = 0;
            int hit = step_bullets(&moved);
            if (moved) {
                changed = 1;
            }
            if (hit) {
                if (g.p1_score >= TARGET_SCORE) {
                    g.game_over = 1;
                    g.winner = 0;
                    clear_bullets();
                } else if (g.p2_score >= TARGET_SCORE) {
                    g.game_over = 1;
                    g.winner = 1;
                    clear_bullets();
                } else {
                    reset_round();
                }
                changed = 1;
            }
        }

        g.tick++;
        rng_next();
        V(SEM_MUTEX);

        if (first_render) {
            changed = 1;
            first_render = 0;
        }

        if (changed) {
            /* 描画要求を出し、完了まで待つ */
            V(SEM_RENDER);
            P(SEM_RENDER_DONE);
        }
    }
}

/* render task */
void task_render(void) {
    while (1) {
        GAMESTATE snapshot;
        P(SEM_RENDER);
        P(SEM_MUTEX);
        /* 描画用に一貫したスナップショットを取る */
        snapshot = g;
        V(SEM_MUTEX);

        render_port(PORT_P1, &snapshot);
        render_port(PORT_P2, &snapshot);
        /* 描画完了を通知 */
        V(SEM_RENDER_DONE);
    }
}

void exit(int status) {
    while (1);
}

int main(void) {
    init_kernel();

    semaphore[SEM_MUTEX].count = 1;
    semaphore[SEM_MUTEX].task_list = NULLTASKID;
    semaphore[SEM_RENDER].count = 0;
    semaphore[SEM_RENDER].task_list = NULLTASKID;
    semaphore[SEM_RENDER_DONE].count = 0;
    semaphore[SEM_RENDER_DONE].task_list = NULLTASKID;
    /* 入力・ゲーム更新・描画の各タスクを登録 */

    set_task(task_input_p1);
    set_task(task_input_p2);
    set_task(task_game);
    set_task(task_render);

    begin_sch();
    return 0;
}
