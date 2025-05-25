#include <libc.h>
#include <stddef.h>

// ! CLONE_THREAD and CLONE_PROCESS
#define CLONE_THREAD 0
#define CLONE_PROCESS 1
#define DEFAULT_STACK_SIZE 1024

// !Game Dimensions
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define SCREEN_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)
#define SCREEN_BUFFER_SIZE (SCREEN_SIZE * sizeof(unsigned short))
#define MAP_WIDTH 78
#define MAP_HEIGHT 23
#define LEVEL_COUNT 3

// ! Characters and colors
#define PACMAN_CHAR '@'
#define GHOST_CHAR 'X'
#define WALL_CHAR '#'
#define FOOD_CHAR '.'
#define POWER_BALL_CHAR 'O'  
#define EMPTY_CHAR ' '

#define COLOR_GAMEOVER 0x0C00  // RED
#define COLOR_WIN 0x0A00       // GREEN

#define COLOR_PACMAN 0x0E00  // YELLOW
#define COLOR_PACMAN_POWER 0x0D00  // MAGENTA
#define COLOR_GHOST_FRIGHTENED 0x0D00 // MAGENTA
#define COLOR_WALL 0x0900    // BLUE
#define COLOR_FOOD 0x0F00    // WHITE
#define COLOR_POWER_BALL 0x0D00  // MAGENTA
#define COLOR_EMPTY 0x0000   // BLACK

// Ghost colors
#define COLOR_BLINKY 0x0C00  // RED
#define COLOR_PINKY  0x0F00  // PINK 
#define COLOR_INKY   0x0B00  // CYAN
#define COLOR_CLYDE  0x0E00  // YELLOW

// ! Directions
#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3
#define RESET 4
#define ESCAPE 5
#define NONE 6

// ! Game states
#define GHOST_NORMAL 0
#define GHOST_FRIGHTENED 1
#define GHOST_DEAD 2

// ! Scene types
typedef enum {
    MENU_SCENE,
    GAME_SCENE,
    GAMEOVER_SCENE,
    WIN_SCENE
} SceneType;

// ! Ghost respawn time 
#define GHOST_RESPAWN_TIME 100

// ! Power ball duration 
#define POWER_BALL_DURATION 100

// ! Bonus message duration
#define BONUS_MESSAGE_DURATION 30

// ! FPS calculation
#define TICKS_PER_SECOND 1800
#define TICKS_PER_GAME_UPDATE 120

// ! Map width level sizes
#define BASE_MAP_WIDTH 39  
#define MAX_MAP_WIDTH 78   // Original maximum width

// ! Input buffer
char input[128];

// ! Keyboard keys
int keys[6] = {0x11, 0x20, 0x1F, 0x1E, 0x13, 0x12}; // w, d, s, a, reset, escape

// -------------- STRUCTURES --------------
// ! Structure to represent an Character (Pacman or Ghost)
typedef struct {
    int x, y;
    int direction;
    char symbol;
    int color;
    int state;           // For ghosts: normal, frightened, dead
    int respawn_timer;   // For ghosts: time until respawn
} Character;

// ! Structure of the game
typedef struct {
    Character pacman;
    Character ghosts[4];
    char map[MAP_HEIGHT][MAX_MAP_WIDTH];
    int score;
    int lives;
    int level;
    int ghost_count;
    int food_count;
    int power_ball_active;    // Power ball timer
    int power_ball_count;     // Number of power balls in the map
    int current_map_width;    // Current map width based on level
} GameState;

// ! Global variables
GameState game;
unsigned short *screen;
int running;
unsigned int frame_count;
unsigned int last_tick;
unsigned int fps;
int game_sem = -1;  // Semaphore for mutual exclusion
SceneType current_scene;  // Current scene

// ! Test results
int test_results[10] = {0};
int current_test = 0;

char buff[24];

int pid;

// ! Direction vectors for movement
const int DIR_VECTORS[4][2] = {
    {0, -1},  // UP
    {1, 0},   // RIGHT
    {0, 1},   // DOWN
    {-1, 0}   // LEFT
};

// ! Pre-calculated positions for ghost respawn
const int GHOST_SPAWN_POSITIONS[4][2] = {
    {2, 2},                    // Top-left
    {BASE_MAP_WIDTH - 3, 2},   // Top-right
    {2, MAP_HEIGHT - 3},       // Bottom-left
    {BASE_MAP_WIDTH - 3, MAP_HEIGHT - 3}  // Bottom-right
};

// ! Ghost colors array for easy access
const int GHOST_COLORS[4] = {
    COLOR_BLINKY,
    COLOR_PINKY,
    COLOR_INKY,
    COLOR_CLYDE
};

/*-------------- MILESTONE FUNCTIONS --------------*/
// 1
int GetKeyboardState(char *keyboard);
int pause(int ms);

// 2
void *StartScreen();

// 3
// int clone(int what, void *(*func)(void*), void *param, int stack_size);
int SetPriority(int priority);
int pthread_exit();

// Wrapper functions
int fork();
int pthread_create(void *(*func)(void*), void *param, int stack_size);

// 4
int sem_init(int value);
int sem_wait(int sem_id);
int sem_post(int sem_id);
int sem_destroy(int sem_id);

/*------------- UTIL FUNCTIONS ------------*/

// ! Absolute value
int abs(int x) {
    return x < 0 ? -x : x;
}

int gettime();

/* ------------ GAME FUNCTIONS ------------ */

void init_game();
void show_menu();
void show_game_over(int is_victory);
void update_game();
void render_game();

// ! Helper functions for position calculations
void handle_wrapping(int *x, int *y, int map_width) {
    if (*x < 0) *x = map_width - 1;
    if (*x >= map_width) *x = 0;
    if (*y < 0) *y = MAP_HEIGHT - 1;
    if (*y >= MAP_HEIGHT) *y = 0;
}

// Calculate ghost corner position based on index and map width
void get_ghost_corner_position(int index, int map_width, int *x, int *y) {
    switch(index) {
        case 0: // Top-left
            *x = 2;
            *y = 2;
            break;
        case 1: // Top-right
            *x = map_width - 3;
            *y = 2;
            break;
        case 2: // Bottom-left
            *x = 2;
            *y = MAP_HEIGHT - 3;
            break;
        case 3: // Bottom-right
            *x = map_width - 3;
            *y = MAP_HEIGHT - 3;
            break;
    }
}

// Check if a position is valid for ghost movement
int is_valid_ghost_position(int x, int y, int ghost_index, Character *ghosts, int ghost_count) {
    // Check wall collision
    if (game.map[y][x] == WALL_CHAR) {
        return 0;
    }
    
    // Check ghost collision
    for (int j = 0; j < ghost_count; j++) {
        if (j != ghost_index && ghosts[j].state != GHOST_DEAD && 
            ghosts[j].x == x && ghosts[j].y == y) {
            return 0;
        }
    }
    
    return 1;
}

// Handle Pacman-ghost collision
void handle_pacman_ghost_collision(Character *ghost, int old_x, int old_y, int new_x, int new_y) {
    if ((ghost->x == game.pacman.x && ghost->y == game.pacman.y) ||  // Direct collision
        (old_x == game.pacman.x && old_y == game.pacman.y) ||        // Ghost moved into Pacman
        (ghost->x == new_x && ghost->y == new_y) ||                  // Ghost and Pacman swapped positions
        (old_x == new_x && old_y == new_y && ghost->x == game.pacman.x && ghost->y == game.pacman.y)) {
        
        if (game.power_ball_active > 0 && ghost->state == GHOST_FRIGHTENED) {
            // Ghost is eaten
            ghost->state = GHOST_DEAD;
            ghost->respawn_timer = GHOST_RESPAWN_TIME;
            game.score += 200;  // Bonus score for eating a ghost
            ghost->x = game.current_map_width / 2;
            ghost->y = MAP_HEIGHT / 2;
        } else if (ghost->state != GHOST_DEAD) {
            // Pacman loses a life
            game.lives--;
            if (game.lives <= 0) {
                show_game_over(0);
                return;
            }
            
            // Respawn Pacman and ghosts
            game.pacman.x = game.current_map_width / 2;
            game.pacman.y = MAP_HEIGHT / 2;
            game.pacman.direction = (int)NULL;
            game.pacman.color = COLOR_PACMAN;
            game.power_ball_active = 0;
            
            // Respawn all ghosts
            for (int j = 0; j < game.ghost_count; j++) {
                get_ghost_corner_position(j, game.current_map_width, &game.ghosts[j].x, &game.ghosts[j].y);
                game.ghosts[j].state = GHOST_NORMAL;
                game.ghosts[j].color = GHOST_COLORS[j];
            }
            
            pause(10);
        }
    }
}

// Calculate best direction for ghost movement
int calculate_best_direction(Character *ghost, int *valid_moves, int num_valid_moves) {
    int best_dir = -1;
    int best_score = (ghost->state == GHOST_FRIGHTENED) ? -1 : game.current_map_width + MAP_HEIGHT;
    
    for (int dir = 0; dir < 4; dir++) {
        if (valid_moves[dir]) {
            int nx = ghost->x + DIR_VECTORS[dir][0];
            int ny = ghost->y + DIR_VECTORS[dir][1];
            handle_wrapping(&nx, &ny, game.current_map_width);
            
            int dist = abs(nx - game.pacman.x) + abs(ny - game.pacman.y);
            
            if (ghost->state == GHOST_FRIGHTENED) {
                if (dist > best_score) {
                    best_score = dist;
                    best_dir = dir;
                }
            } else {
                if (dist < best_score) {
                    best_score = dist;
                    best_dir = dir;
                }
            }
        }
    }
    
    return best_dir;
}

// Initialize game state
void init_game() {
    // Initialize running variables
    running = 1;
    frame_count = 0;
    last_tick = gettime();
    fps = 0;

    // Initialize game state
    game.score = 0;
    game.lives = 3;
    game.level = 1;
    
    // Calculate current map width based on level
    game.current_map_width = BASE_MAP_WIDTH;
    game.current_map_width += BASE_MAP_WIDTH * (game.level-1) * 1/game.level;
    if (game.current_map_width > MAX_MAP_WIDTH || game.level >= 5) {
        game.current_map_width = MAX_MAP_WIDTH;
    }

    // Initialize Pacman
    game.pacman.x = game.current_map_width / 2;
    game.pacman.y = MAP_HEIGHT / 2;
    game.pacman.direction = NONE;
    game.pacman.symbol = PACMAN_CHAR;
    game.pacman.color = COLOR_PACMAN;
    
    // Initialize ghosts
    game.ghost_count = 4;
    for (int i = 0; i < game.ghost_count; i++) {
        get_ghost_corner_position(i, game.current_map_width, &game.ghosts[i].x, &game.ghosts[i].y);
        game.ghosts[i].direction = i % 4;
        game.ghosts[i].symbol = GHOST_CHAR;
        game.ghosts[i].color = GHOST_COLORS[i];
        game.ghosts[i].state = GHOST_NORMAL;
        game.ghosts[i].respawn_timer = 0;
    }
    
    // Initialize map 
    game.food_count = 0;
    game.power_ball_count = 0;
    game.power_ball_active = 0;

    // Clear the entire map first
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAX_MAP_WIDTH; x++) {
            game.map[y][x] = EMPTY_CHAR;
        }
    }
    
    // Generate map for current width
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < game.current_map_width; x++) {
            // Border walls
            if (x == 0 || y == 0 || x == game.current_map_width-1 || y == MAP_HEIGHT-1) {
                game.map[y][x] = WALL_CHAR;
            }
            // Interior walls with much larger gaps
            else if (
                // Top area walls 
                (y == 5 && (x < 8 || (x > 15 && x < 25) || (x > 35 && x < 45) || (x > 55 && x < 65) || x > game.current_map_width-6)) ||
                (y == 8 && (x < 8 || (x > 15 && x < 25) || (x > 35 && x < 45) || (x > 55 && x < 65) || x > game.current_map_width-6)) ||
                
                // Bottom area walls 
                (y == MAP_HEIGHT-6 && (x < 8 || (x > 15 && x < 25) || (x > 35 && x < 45) || (x > 55 && x < 65) || x > game.current_map_width-6)) ||
                (y == MAP_HEIGHT-9 && (x < 8 || (x > 15 && x < 25) || (x > 35 && x < 45) || (x > 55 && x < 65) || x > game.current_map_width-6)) ||
                
                // Center area walls 
                (x == 25 && y > 8 && y < 16 && (y < 10 || y > 14)) ||
                (x == 35 && y > 8 && y < 16 && (y < 10 || y > 14)) ||
                (x == 45 && y > 8 && y < 16 && (y < 10 || y > 14)) ||
                (x == 55 && y > 8 && y < 16 && (y < 10 || y > 14)) ||
                
                // Horizontal center walls 
                (y == 10 && x > 25 && x < 55 && (x < 30 || x > 50)) ||
                (y == 14 && x > 25 && x < 55 && (x < 30 || x > 50)) ||
                
                // Ghost house walls 
                (x == 20 && y > 5 && y < 10 && (y < 6 || y > 9)) ||
                (x == game.current_map_width-20 && y > 5 && y < 10 && (y < 6 || y > 9)) ||
                (x == 20 && y > 15 && y < 20 && (y < 16 || y > 19)) ||
                (x == game.current_map_width-20 && y > 15 && y < 20 && (y < 16 || y > 19))
            ) {
                game.map[y][x] = WALL_CHAR;
            }
            // Power balls in strategic locations
            else if (
                // Corner power balls
                (x == 2 && y == 2) || 
                (x == game.current_map_width-3 && y == 2) ||
                (x == 2 && y == MAP_HEIGHT-3) ||
                (x == game.current_map_width-3 && y == MAP_HEIGHT-3) ||
                // Center power balls
                (x == game.current_map_width/2 && y == MAP_HEIGHT/2) ||
                (x == game.current_map_width/2 - 10 && y == MAP_HEIGHT/2) ||
                (x == game.current_map_width/2 + 10 && y == MAP_HEIGHT/2) ||
                // Strategic power balls
                (x == 15 && y == 15) ||
                (x == game.current_map_width-15 && y == 15) ||
                (x == 15 && y == MAP_HEIGHT-15) ||
                (x == game.current_map_width-15 && y == MAP_HEIGHT-15)
            ) {
                game.map[y][x] = POWER_BALL_CHAR;
                game.power_ball_count++;
            }
            // Food
            else {
                game.map[y][x] = FOOD_CHAR;
                game.food_count++;
            }
        }
    }
}

// Function to show Game Over or Victory screen
void show_game_over(int is_victory) {
    // Set the appropriate scene
    current_scene = is_victory ? WIN_SCENE : GAMEOVER_SCENE;
    
    // Clear the screen
    for (int i = 0; i < SCREEN_SIZE; i++) {
        screen[i] = COLOR_EMPTY | ' ';
    }
    
    // Show "GAME OVER!" or "YOU WIN!" in the center
    char *message = is_victory ? "YOU WIN!" : "GAME OVER!";
    int center_x = (SCREEN_WIDTH - strlen(message)) / 2;
    int center_y = SCREEN_HEIGHT / 2 - 2;
    
    for (int i = 0; message[i]; i++) {
        int pos = center_y * SCREEN_WIDTH + center_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = (is_victory ? COLOR_WIN : COLOR_GAMEOVER) | message[i];
        }
    }
    
    // Show score
    char score_text[50] = "Final Score: ";
    itoa(game.score, &score_text[13]);
    int score_x = (SCREEN_WIDTH - strlen(score_text)) / 2;
    
    for (int i = 0; score_text[i]; i++) {
        int pos = (center_y + 1) * SCREEN_WIDTH + score_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_FOOD | score_text[i];
        }
    }
    
    // Show options
    char restart[] = "Press 'R' to restart";
    char exit[] = "Press 'E' to exit";
    
    int restart_x = (SCREEN_WIDTH - strlen(restart)) / 2;
    int exit_x = (SCREEN_WIDTH - strlen(exit)) / 2;
    
    // Show restart option
    for (int i = 0; restart[i]; i++) {
        int pos = (center_y + 3) * SCREEN_WIDTH + restart_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_FOOD | restart[i];
        }
    }
    
    // Show exit option
    for (int i = 0; exit[i]; i++) {
        int pos = (center_y + 4) * SCREEN_WIDTH + exit_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_FOOD | exit[i];
        }
    }
    
    // Wait for user to decide what to do
    while (running) {
        if (GetKeyboardState(input) == 0) {
            if (input[keys[RESET]]) {
                // Reset timing variables
                last_tick = gettime();
                frame_count = 0;
                fps = 0;
                
                // Restart game
                write(1, "Restarting game...\n", 19);
                current_scene = GAME_SCENE;
                init_game();
                break;
            }
            else if (input[keys[ESCAPE]]) { 
                running = 0;
                write(1, "Exiting game...\n", 16);
                break;
            }
        }
        pause(10);  // Add pause to avoid busy waiting
    }
}

// Update game state
void update_game() {
    // Update power ball timer
    if (game.power_ball_active > 0) {
        game.power_ball_active--;
        if (game.power_ball_active == 0) {
            for (int i = 0; i < game.ghost_count; i++) {
                if (game.ghosts[i].state != GHOST_DEAD) {
                    game.ghosts[i].state = GHOST_NORMAL;
                    game.ghosts[i].color = GHOST_COLORS[i];
                }
            }
            game.pacman.color = COLOR_PACMAN;
        }
    }

    // Update Pacman
    int new_x = game.pacman.x + DIR_VECTORS[game.pacman.direction][0];
    int new_y = game.pacman.y + DIR_VECTORS[game.pacman.direction][1];
    handle_wrapping(&new_x, &new_y, game.current_map_width);
    
    if (game.map[new_y][new_x] != WALL_CHAR) {
        game.pacman.x = new_x;
        game.pacman.y = new_y;
        
        char tile = game.map[new_y][new_x];
        if (tile == FOOD_CHAR) {
            game.map[new_y][new_x] = EMPTY_CHAR;
            game.score += 10;
            game.food_count--;
            
            if (game.food_count <= 0) {
                game.level++;
                if (game.level > LEVEL_COUNT) {
                    show_game_over(1);
                    return;
                }
                init_game();
                return;
            }
        }
        else if (tile == POWER_BALL_CHAR) {
            game.map[new_y][new_x] = EMPTY_CHAR;
            game.score += 50;
            game.power_ball_count--;
            game.power_ball_active = POWER_BALL_DURATION;
            game.pacman.color = COLOR_PACMAN_POWER;
            
            for (int i = 0; i < game.ghost_count; i++) {
                if (game.ghosts[i].state != GHOST_DEAD) {
                    game.ghosts[i].state = GHOST_FRIGHTENED;
                    game.ghosts[i].color = COLOR_GHOST_FRIGHTENED;
                }
            }
        }
    }
    
    // Update ghosts
    for (int i = 0; i < game.ghost_count; i++) {
        Character *ghost = &game.ghosts[i];
        
        // Handle ghost respawn
        if (ghost->state == GHOST_DEAD) {
            ghost->respawn_timer--;
            if (ghost->respawn_timer <= 0) {
                get_ghost_corner_position(i, game.current_map_width, &ghost->x, &ghost->y);
                ghost->state = GHOST_NORMAL;
                ghost->color = GHOST_COLORS[i];
                ghost->direction = i % 4;
                continue;
            }
            continue;
        }
        
        // Calculate valid moves
        int valid_moves[4] = {0};
        int num_valid_moves = 0;
        
        for (int dir = 0; dir < 4; dir++) {
            int nx = ghost->x + DIR_VECTORS[dir][0];
            int ny = ghost->y + DIR_VECTORS[dir][1];
            handle_wrapping(&nx, &ny, game.current_map_width);
            
            if (is_valid_ghost_position(nx, ny, i, game.ghosts, game.ghost_count)) {
                valid_moves[dir] = 1;
                num_valid_moves++;
            }
        }
        
        // Choose direction
        int new_dir;
        if (num_valid_moves == 0) {
            new_dir = (ghost->direction + 2) % 4;
            int nx = ghost->x + DIR_VECTORS[new_dir][0];
            int ny = ghost->y + DIR_VECTORS[new_dir][1];
            handle_wrapping(&nx, &ny, game.current_map_width);
            
            if (!is_valid_ghost_position(nx, ny, i, game.ghosts, game.ghost_count)) {
                for (int dir = 0; dir < 4; dir++) {
                    nx = ghost->x + DIR_VECTORS[dir][0];
                    ny = ghost->y + DIR_VECTORS[dir][1];
                    handle_wrapping(&nx, &ny, game.current_map_width);
                    
                    if (is_valid_ghost_position(nx, ny, i, game.ghosts, game.ghost_count)) {
                        new_dir = dir;
                        break;
                    }
                }
            }
        }
        else if (ghost->state == GHOST_FRIGHTENED) {
            if (gettime() % 3 == 0) {
                do {
                    new_dir = gettime() % 4;
                } while (!valid_moves[new_dir]);
            } else {
                new_dir = calculate_best_direction(ghost, valid_moves, num_valid_moves);
            }
        }
        else if (gettime() % 10 < 7 && num_valid_moves > 1) {
            new_dir = calculate_best_direction(ghost, valid_moves, num_valid_moves);
        } else {
            do {
                new_dir = gettime() % 4;
            } while (!valid_moves[new_dir]);
        }
        
        // Move ghost
        int new_ghost_x = ghost->x + DIR_VECTORS[new_dir][0];
        int new_ghost_y = ghost->y + DIR_VECTORS[new_dir][1];
        handle_wrapping(&new_ghost_x, &new_ghost_y, game.current_map_width);
        
        if (is_valid_ghost_position(new_ghost_x, new_ghost_y, i, game.ghosts, game.ghost_count)) {
            int old_x = ghost->x;
            int old_y = ghost->y;
            
            ghost->direction = new_dir;
            ghost->x = new_ghost_x;
            ghost->y = new_ghost_y;
            
            handle_pacman_ghost_collision(ghost, old_x, old_y, new_x, new_y);
        }
    }
}

// Render game state to screen
void render_game() {
    int map_offset = (SCREEN_WIDTH - game.current_map_width) / 2;

    // Clear screen
    for (int i = 0; i < SCREEN_SIZE; i++) {
        screen[i] = COLOR_EMPTY | ' ';
    }
    
    // Draw map
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < game.current_map_width; x++) {
            char tile = game.map[y][x];
            int color = COLOR_EMPTY;
            
            switch (tile) {
                case WALL_CHAR: color = COLOR_WALL; break;
                case FOOD_CHAR: color = COLOR_FOOD; break;
                case POWER_BALL_CHAR: color = COLOR_POWER_BALL; break;
                default: color = COLOR_EMPTY; break;
            }
            
            // Calculate screen position with bounds checking
            int screen_pos = (y+1) * SCREEN_WIDTH + (x+1) + map_offset;
            if (screen_pos >= 0 && screen_pos < SCREEN_SIZE) {
                screen[screen_pos] = color | tile;
            }
        }
    }
    
    // Draw Pacman with bounds checking
    int pacman_pos = (game.pacman.y+1) * SCREEN_WIDTH + (game.pacman.x+1) + map_offset;
    if (pacman_pos >= 0 && pacman_pos < SCREEN_SIZE) {
        screen[pacman_pos] = game.pacman.color | game.pacman.symbol;
    }
    
    // Draw ghosts with bounds checking
    for (int i = 0; i < game.ghost_count; i++) {
        Character *ghost = &game.ghosts[i];
        int ghost_pos = (ghost->y+1) * SCREEN_WIDTH + (ghost->x+1) + map_offset;
        if (ghost_pos >= 0 && ghost_pos < SCREEN_SIZE) {
            screen[ghost_pos] = ghost->color | ghost->symbol;
        }
    }

    // Display FPS
    char fps_text[20] = "FPS: ";
    itoa(fps, &fps_text[5]);

    for (int i = 0; fps_text[i]; i++) {
        screen[i] = COLOR_PACMAN | fps_text[i];
    }
    
    // Draw score, lives, and level with bounds checking
    char score_text[20] = "Score: ";
    itoa(game.score, &score_text[7]);
    
    for (int i = 0; score_text[i] && i < 20; i++) {
        int pos = SCREEN_WIDTH * (SCREEN_HEIGHT-1) + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_FOOD | score_text[i];
        }
    }
    
    char lives_text[20] = "Lives: ";
    itoa(game.lives, &lives_text[7]);
    
    for (int i = 0; lives_text[i] && i < 20; i++) {
        int pos = SCREEN_WIDTH * (SCREEN_HEIGHT-1) + 40 + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_FOOD | lives_text[i];
        }
    }
    
    char level_text[20] = "Level: ";
    itoa(game.level, &level_text[7]);
    
    for (int i = 0; level_text[i] && i < 20; i++) {
        int pos = SCREEN_WIDTH * (SCREEN_HEIGHT-1) + 60 + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_FOOD | level_text[i];
        }
    }
}

// Function to show menu screen
void show_menu() {
    current_scene = MENU_SCENE;
    
    // Clear the screen
    for (int i = 0; i < SCREEN_SIZE; i++) {
        screen[i] = COLOR_EMPTY | ' ';
    }
    
    // ASCII art for PACZEOS using simple characters
    char *title[] = {
        "                                                ",
        "  _____ _____ _____ _____ _____ _____ _____     ",
        " |  _  |  _  |     |__   |   __|     |   __|    ",
        " |   __|     |   --|   __|   __|  |  |__   |    ",
        " |__|  |__|__|_____|_____|_____|_____|_____|    ",
        "                                                    "
    };
    
    // Draw ASCII art title
    int title_y = SCREEN_HEIGHT / 2 - 6;
    for (int i = 0; i < 6; i++) {
        int title_x = (SCREEN_WIDTH - strlen(title[i])) / 2;
        for (int j = 0; title[i][j]; j++) {
            int pos = (title_y + i) * SCREEN_WIDTH + title_x + j;
            if (pos >= 0 && pos < SCREEN_SIZE) {
                screen[pos] = COLOR_PACMAN | title[i][j];
            }
        }
    }
    
    // Show controls message
    char controls_msg[] = "Controls: W (up), A (left), S (down), D (right), E (exit), R (restart)";
    int controls_x = (SCREEN_WIDTH - strlen(controls_msg)) / 2;
    int controls_y = SCREEN_HEIGHT / 2 + 2;
    
    for (int i = 0; controls_msg[i]; i++) {
        int pos = controls_y * SCREEN_WIDTH + controls_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_WALL | controls_msg[i];  // Blue color
        }
    }
    
    // Show "Press any key to start" message
    char start_msg[] = "Press any key to start";
    int msg_x = (SCREEN_WIDTH - strlen(start_msg)) / 2;
    int msg_y = SCREEN_HEIGHT / 2 + 4;
    
    for (int i = 0; start_msg[i]; i++) {
        int pos = msg_y * SCREEN_WIDTH + msg_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_FOOD | start_msg[i];    // White color
        }
    }

    // Show authors and year at the bottom of the screen
    char *authors = "Sergio Shmyhelskyy Yaskevych && Alex LaFuente Gonzalez";
    char *year = "2025";
    int authors_x = (SCREEN_WIDTH - strlen(authors)) / 2;
    int year_x = (SCREEN_WIDTH - strlen(year)) / 2;
    int authors_y = SCREEN_HEIGHT - 3;  // 2 lines from bottom
    
    // Draw authors
    for (int i = 0; authors[i]; i++) {
        int pos = authors_y * SCREEN_WIDTH + authors_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_PACMAN_POWER | authors[i];  // Magenta color
        }
    }
    
    // Draw year
    for (int i = 0; year[i]; i++) {
        int pos = (authors_y + 1) * SCREEN_WIDTH + year_x + i;
        if (pos >= 0 && pos < SCREEN_SIZE) {
            screen[pos] = COLOR_PACMAN_POWER | year[i];  // Magenta color
        }
    }
    
    // Wait for any key press
    while (current_scene == MENU_SCENE) {
        if (GetKeyboardState(input) == 0) {
            for (int i = 0; i < 128; i++) {
                if (input[i]) {
                    // Print a message to the console
                    write(1, "Starting game...\n", 17);
                    
                    // Set the current scene to the game scene
                    current_scene = GAME_SCENE;

                    return;  // Any key pressed, exit menu
                }
            }
        }
    }
}

/*---------------- GAME THREADS ------------*/

// ! Input thread for keyboard handling (main thread)
void *input_thread(void *arg) {    
    // Set high priority for keyboard thread
    SetPriority(35);  // Highest priority

    while (running) {
        // Check if keyboard is pressed
        if (GetKeyboardState(input) == 0) {
            // Only process game controls if we're in the game scene
            if (current_scene == GAME_SCENE) {
                // Update Pacman's direction based on input
                if (input[keys[UP]]) {  
                    game.pacman.direction = UP;
                }
                else if (input[keys[RIGHT]]) {  
                    game.pacman.direction = RIGHT;
                }
                else if (input[keys[DOWN]]) {  
                    game.pacman.direction = DOWN;
                }
                else if (input[keys[LEFT]]) {  
                    game.pacman.direction = LEFT;
                }
            }
            
            // Global controls (work in any scene)
            if (input[keys[ESCAPE]]) {
                running = 0;
                write(1, "Exiting game...\n", 16);
            }

            // Restart game if 'R' is pressed 
            else if (input[keys[RESET]]) { // [CHEAT CODE: can restart during game]
                init_game();
            }
        }
        
        // Pause for a short time to avoid busy waiting
        pause(1);
    }
    
    pthread_exit();
    return NULL;
}

// ! Game thread for updating and rendering
void *game_thread(void *arg) {
    // Set medium priority for game thread
    SetPriority(25);  // Medium priority
    
    // Initialize timing variables for FPS calculation
    last_tick = gettime();
    frame_count = fps = 0;
    
    while (running) {
        ++frame_count;  

        // ? Update game state 
        if (gettime() - last_tick >= TICKS_PER_GAME_UPDATE) {
            // ? Update FPS
            last_tick = gettime();
            fps = frame_count / TICKS_PER_SECOND;
            frame_count = 0;

            sem_wait(game_sem); // Lock the game state

            update_game();

            render_game();
            
            sem_post(game_sem); // Unlock the game state
        }
    }
    
    pthread_exit();
    return NULL;
}

/* ------------ TESTING THREADS ------------ */

// ! Thread function for testing
void *thread_function(void *arg) {
    if (*(int *)arg == 1) {
        write(1, "Hello from thread 1\n", 22);
    }
    pthread_exit();
    return NULL;    // Never reached, but good practice
}

// ! Thread function for testing
void *thread_func_N(void *arg) {
    char *msg = (char *)arg;
    write(1, msg, strlen(msg));
    pthread_exit();
    return NULL;    // Never reached, but good practice
}

// ! Thread function for testing
void *priority_func(void *arg) {
    int value = *((int*)arg);   // race condition...
    if (value == 0) {
        SetPriority(25);   // LOW
        pause(1);
    }
    else
        SetPriority(30);  // HIGH
    
    if (value == 0)
	    for (int i = 0; i < 10; ++i) 
        	write(1, "LOW\n", 4);
    else
	    for (int i = 0; i < 10; ++i) 
        	write(1, "HIGH\n", 5);

    pthread_exit();
    return NULL;    // Never reached, but good practice
}

/* ------------ TESTING FUNCTIONS ------------ */

// Test simple clone
int test_clone_simple() {
    int value = 1;
    int tid = pthread_create(thread_function, &value, 1024);
    if (tid < 0) {
      write(1, "Error cloning thread\n", 22);
      perror();
    }
    pause(50);  // Wait for thread to finish
    write(1, "Thread cloned successfully\n", 27);
    
    return 1;
}

// Test multiple threads
int test_clone_multiple() {
    int tid = pthread_create(thread_func_N, "Thread A\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    tid = pthread_create(thread_func_N, "Thread B\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    tid = pthread_create(thread_func_N, "Thread C\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    pause(200);  // Wait for threads to finish
    
    return 1;
}

// Test priority
int test_priority() {
    int valueA = 0;
    pthread_create(priority_func, &valueA, 1024);  // LOW
    int valueB = 1;
    pthread_create(priority_func, &valueB, 1024);  // HIGH
    pause(1);

    return 1;
}

// Test semaphore initialization
int test_sem_init() {
    int sem_id = sem_init(1);
    if (sem_id < 0) {
        write(1, "Error initializing semaphore\n", 30);
        return 0;
    }
    write(1, "Semaphore initialized successfully\n", 35);
    return 1;
}

// Test semaphore wait/post
int test_sem_wait_post() {
    int sem_id = sem_init(1);
    if (sem_id < 0) return 0;
    
    // Test wait
    if (sem_wait(sem_id) < 0) {
        write(1, "Error in sem_wait\n", 19);
        return 0;
    }
    
    // Test post
    if (sem_post(sem_id) < 0) {
        write(1, "Error in sem_post\n", 19);
        return 0;
    }
    
    write(1, "Semaphore wait/post test passed\n", 32);
    return 1;
}

// Test semaphore blocking
int test_sem_blocking() {
    int sem_id = sem_init(0);  // Initialize with 0 to force blocking
    if (sem_id < 0) return 0;
    
    // Create a thread that will block on the semaphore
    int tid = pthread_create(thread_function, (void*)sem_id, 1024);
    if (tid < 0) {
        write(1, "Error creating test thread\n", 28);
        return 0;
    }
    
    // Wait a bit to ensure the thread is blocked
    pause(50);
    
    // Post to unblock the thread
    if (sem_post(sem_id) < 0) {
        write(1, "Error in sem_post\n", 19);
        return 0;
    }
    
    write(1, "Semaphore blocking test passed\n", 32);
    return 1;
}

// Thread function for blocking test
void *test_sem_blocking_thread(void *arg) {
    int sem_id = (int)arg;
    
    // This should block
    if (sem_wait(sem_id) < 0) {
        write(1, "Error in sem_wait\n", 19);
        return NULL;
    }
    
    write(1, "Thread unblocked successfully\n", 31);
    return NULL;
}

// Test semaphore destruction
int test_sem_destroy() {
    int sem_id = sem_init(1);
    if (sem_id < 0) return 0;
    
    // Test destroy
    if (sem_destroy(sem_id) < 0) {
        write(1, "Error in sem_destroy\n", 21);
        return 0;
    }
    
    write(1, "Semaphore destroy test passed\n", 31);
    return 1;
}

// Run all semaphore tests
int run_semaphore_tests() {
    write(1, "\nRunning semaphore tests...\n", 28);
    
    test_results[0] = test_sem_init();
    test_results[1] = test_sem_wait_post();
    test_results[2] = test_sem_blocking();
    test_results[3] = test_sem_destroy();
    
    // Print results
    write(1, "\nSemaphore test results:\n", 25);
    for (int i = 0; i < 4; i++) {
        char result[50];
        itoa(test_results[i], result);
        write(1, "Test ", 5);
        itoa(i + 1, &result[5]);
        write(1, result, strlen(result));
        write(1, ": ", 2);
        write(1, test_results[i] ? "PASSED\n" : "FAILED\n", test_results[i] ? 7 : 7);
    }
    
    return 1;
}

/* ------------ MAIN ------------ */

int __attribute__ ((__section__(".text.main")))
    main(void) 
{
    write(1, "\nInitializing PacZeOS...\n", 25);
    
    // Initialize screen
    screen = (unsigned short *)StartScreen();
    if (screen == (void*)-1) {
        write(1, "Error initializing screen\n", 27);
        return -1;
    }
    
    // Clear screen initially and verify memory access
    for (int i = 0; i < SCREEN_SIZE; i++) {
        if (screen != NULL) {
            screen[i] = COLOR_EMPTY | ' ';
        }
    }
    
    // Show menu screen before starting the game
    show_menu();
    
    // Initialize game state
    init_game();

    // Set the lowest priority for this process
    SetPriority(1);
    
    // Initialize semaphore for mutual exclusion
    game_sem = sem_init(1);  // Binary semaphore
    if (game_sem < 0) {
        write(1, "Error initializing semaphore\n", 30);
        return -1;
    }
    
    // Create keyboard thread (main thread)
    int tid1 = pthread_create(input_thread, NULL, 1024);
    if (tid1 < 0) {
        write(1, "Error creating keyboard thread\n", 31);
        sem_destroy(game_sem);
        return -1;
    }
    
    // Create game thread
    int tid2 = pthread_create(game_thread, NULL, 1024);
    if (tid2 < 0) {
        write(1, "Error creating game thread\n", 27);
        sem_destroy(game_sem);
        return -1;
    }
    
    // Main thread waits for game to end
    while (running);
    
    // Clean up semaphore
    sem_destroy(game_sem);
    
    // Game over, show final score
    write(1, "Game Over!\n", 11);
    write(1, "Final Score: ", 13);
    itoa(game.score, buff);
    write(1, buff, strlen(buff));
    write(1, "\n", 1);
    
    return 0;
}