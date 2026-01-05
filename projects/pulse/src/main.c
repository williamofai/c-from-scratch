/**
 * main.c - Example usage of pulse liveness monitor
 * 
 * This demonstrates basic usage of the pulse API.
 * In production, you would integrate with your actual
 * heartbeat source (pipes, signals, sockets, etc.)
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#define _XOPEN_SOURCE 500  /* For usleep */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "pulse.h"

/** Get current time in milliseconds (monotonic) */
static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + 
           (uint64_t)ts.tv_nsec / 1000000ULL;
}

/** Convert state to string for display */
static const char* state_name(state_t st)
{
    switch (st) {
        case STATE_UNKNOWN: return "UNKNOWN";
        case STATE_ALIVE:   return "ALIVE";
        case STATE_DEAD:    return "DEAD";
        default:            return "INVALID";
    }
}

int main(void)
{
    hb_fsm_t monitor;
    uint64_t T = 2000;  /* 2 second timeout */
    uint64_t W = 500;   /* 0.5 second init window */
    
    printf("pulse - Heartbeat Liveness Monitor Demo\n");
    printf("========================================\n");
    printf("Timeout (T): %lu ms\n", (unsigned long)T);
    printf("Init window (W): %lu ms\n\n", (unsigned long)W);
    
    /* Initialize */
    hb_init(&monitor, now_ms());
    printf("[%8lu] Initialized: state = %s\n", 
           (unsigned long)now_ms(), state_name(hb_state(&monitor)));
    
    /* Simulate: heartbeat every 500ms for 5 beats, then stop */
    printf("\n--- Sending heartbeats every 500ms ---\n");
    for (int i = 0; i < 5; i++) {
        usleep(500000);  /* 500ms */
        hb_step(&monitor, now_ms(), 1, T, W);  /* heartbeat seen */
        printf("[%8lu] Heartbeat #%d: state = %s\n", 
               (unsigned long)now_ms(), i + 1, 
               state_name(hb_state(&monitor)));
    }
    
    /* Simulate: no heartbeats, watch timeout */
    printf("\n--- Stopping heartbeats, watching for timeout ---\n");
    for (int i = 0; i < 6; i++) {
        usleep(500000);  /* 500ms */
        hb_step(&monitor, now_ms(), 0, T, W);  /* no heartbeat */
        printf("[%8lu] No heartbeat: state = %s%s\n", 
               (unsigned long)now_ms(),
               state_name(hb_state(&monitor)),
               hb_faulted(&monitor) ? " [FAULT]" : "");
        
        if (hb_state(&monitor) == STATE_DEAD) {
            printf("\n*** Process declared DEAD after timeout ***\n");
            break;
        }
    }
    
    /* Simulate: recovery */
    printf("\n--- Heartbeat resumes (recovery) ---\n");
    usleep(500000);
    hb_step(&monitor, now_ms(), 1, T, W);  /* heartbeat seen */
    printf("[%8lu] Heartbeat received: state = %s\n", 
           (unsigned long)now_ms(), state_name(hb_state(&monitor)));
    
    printf("\nDemo complete.\n");
    return 0;
}
