#include <libc.h>
#include <stddef.h>

#define CLONE_THREAD 1
#define CLONE_PROCESS 0

char buff[24];
char char_map[] = 
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','�','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','�',
  '\0','�','\0','�','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

int pid;

// Test results
int test_results[10] = {0};
int current_test = 0;

// Test keyboard buffer
char keyboard[128];

// Test screen buffer
unsigned short *screen;

/* ------------ THREADS ------------ */

// Main thread for keyboard input
void *keyboard_thread(void *arg) {
    // TODO: Implement keyboard thread
    char keyboard[128];
    while (1) {
        GetKeyboardState(keyboard);
        // Process keyboard input
        pause(16); // ~60 FPS
    }
}

// Game thread for movement and rendering
void *game_thread(void *arg) {
  // TODO: Implement game thread
}

void *thread_function(void *arg) {
    if (*(int *)arg == 1) {
        write(1, "Hello from thread 1\n", 22);
    }
    pthread_exit();
    return NULL;  // Never reached, but good practice
}

void *thread_func_N(void *arg) {
    char *msg = (char *)arg;
    write(1, arg, strlen(arg));
    pthread_exit();
}

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
}

/* ------------ TESTING ------------ */
int test_simple_clone() {
    int value = 1;
    int tid = clone(CLONE_THREAD, thread_function, &value, 1024);
    if (tid < 0) {
      write(1, "Error cloning thread\n", 22);
      perror();
    }
    pause(50);  // Wait for thread to finish
    write(1, "Thread cloned successfully\n", 27);
    
    return 1;
}

int test_multiple_threads() {
    int tid = clone(CLONE_THREAD, thread_func_N, "Thread A\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    tid = clone(CLONE_THREAD, thread_func_N, "Thread B\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    tid = clone(CLONE_THREAD, thread_func_N, "Thread C\n", 1024);
    if (tid < 0) write(1, "clone failed\n", 13);
    pause(200);  // Wait for threads to finish
    
    return 1;
}

int test_priority() {
    int valueA = 0;
    clone(CLONE_THREAD, priority_func, &valueA, 1024);  // LOW
    int valueB = 1;
    clone(CLONE_THREAD, priority_func, &valueB, 1024);  // HIGH
    pause(1);
    write(1, "HIGH GOES BEFORE, RIGHT?\n", 25);

    return 1;
}

/* ------------ UTIL FUNCTIONS ------------ */

void update_fps(char *screen, int fps) {
    const char *prefix = "FPS: ";
    int i;
    
    // Write prefix
    for (i = 0; prefix[i] != '\0'; i++) {
        screen[i] = 0x0F00 | prefix[i];
    }
    
    // Write FPS number
    print_number_to_screen(fps, (unsigned short *)screen, i);
}

int __attribute__ ((__section__(".text.main")))
main(void) {
    write(1, "\n", 1);

    // Initialize screen first
    write(1, "Initializing screen...\n", 23);
    unsigned short *screen = (unsigned short *)StartScreen();
    
    if (screen == (void*)-1) {
        write(1, "Failed to initialize screen\n", 29);
        return -1;
    }
    
    write(1, "Screen initialized successfully\n", 32);
    
    // Clear screen with a pattern to verify it's working
    for (int i = 0; i < 80*25; i++) {
        screen[i] = 0x0F00 | '#';  // Fill with # to verify screen is working
    }
    
    // Initialize keyboard buffer
    for (int i = 0; i < 128; i++) {
        keyboard[i] = 0;
    }
    
    write(1, "Starting tests...\n", 18);
    
    test_simple_clone();
    test_multiple_threads();
    test_priority();

    write(1, "Tests completed\n", 16);
    
    // Main loop for interactive testing
    while (1) {
        if (GetKeyboardState(keyboard) < 0) {
            write(1, "Keyboard error\n", 15);
        }
        
        // Display pressed keys
        if (screen != (void*)-1) {
            // Clear previous key display area
            for (int i = 0; i < 80; i++) {
                screen[i] = 0x0F00 | ' ';
            }
            
            // Show pressed keys
            for (int i = 0; i < 128; i++) {
                if (keyboard[i]) {
                    screen[i % 80] = 0x0F00 | char_map[i];
                }
            }
        }
        
        pause(16);  // ~60 FPS
    }
    
    return 0;
}
