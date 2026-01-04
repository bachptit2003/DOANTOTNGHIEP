#include "utils.h"
#include "gateway.h"
#include 
#include 
#include 

void get_timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%H:%M:%S", t);
}

void signal_handler(int sig) {
    static int force_quit = 0;
    
    if (force_quit) {
        printf("\n[FORCE] Force quitting...\n");
        exit(1);
    }
    printf("\n\n[SIGNAL] Shutting down...\n");
    gateway.running = 0;
    force_quit = 1;
}

void shutdown_timeout_handler(int sig) {
    printf("\n[TIMEOUT] Shutdown timeout - force exit!\n");
    exit(1);
}