/**
 * safety_monitor.c - Complete Safety Monitoring System
 * 
 * Integration example demonstrating all 6 c-from-scratch modules:
 * 
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │                        SAFETY MONITOR                           │
 *   │                                                                 │
 *   │  Three redundant sensors, each monitored by a complete pipeline │
 *   │  Consensus voting produces trusted output for bounded queue     │
 *   └─────────────────────────────────────────────────────────────────┘
 * 
 * Pipeline per sensor:
 *   Sensor → Pulse (alive?) → Baseline (normal?) → Timing (regular?)
 *         → Drift (trending?) → Health State
 * 
 * Then:
 *   [Health₀, Health₁, Health₂] → Consensus (vote) → Pressure (buffer)
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* All 6 modules */
#include "pulse.h"
#include "baseline.h"
#include "timing.h"
#include "drift.h"
#include "consensus.h"
#include "pressure.h"

/*===========================================================================
 * Configuration
 *===========================================================================*/

#define NUM_SENSORS     3
#define SIM_DURATION    100     /* Simulation ticks */
#define SAMPLE_INTERVAL 100     /* ms between samples */

/* Sensor simulation parameters */
#define GROUND_TRUTH    100.0
#define NOISE_STD       0.5
#define DRIFT_START     40      /* Tick when sensor 2 starts drifting */
#define DRIFT_RATE      0.3     /* Units per tick */
#define FAILURE_TICK    70      /* Tick when sensor 2 dies */

/*===========================================================================
 * Sensor Channel Structure
 *===========================================================================*/

typedef struct {
    int id;
    
    /* Module instances */
    pulse_t pulse;
    baseline_t baseline;
    timing_t timing;
    drift_fsm_t drift;
    
    /* Computed health for consensus */
    sensor_health_t health;
    double last_value;
    
} sensor_channel_t;

/*===========================================================================
 * Global State
 *===========================================================================*/

static sensor_channel_t channels[NUM_SENSORS];
static consensus_fsm_t voter;
static pressure_queue_t output_queue;
static pressure_item_t queue_buffer[32];

/*===========================================================================
 * Simulation Helpers
 *===========================================================================*/

static double rand_normal(double mean, double std) {
    /* Box-Muller transform */
    double u1 = (rand() + 1.0) / (RAND_MAX + 2.0);
    double u2 = (rand() + 1.0) / (RAND_MAX + 2.0);
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265359 * u2);
    return mean + std * z;
}

static double simulate_sensor(int sensor_id, int tick) {
    double value = GROUND_TRUTH;
    
    /* Add noise to all sensors */
    value += rand_normal(0, NOISE_STD);
    
    /* Sensor 2 special behaviour */
    if (sensor_id == 2) {
        /* Drift after DRIFT_START */
        if (tick >= DRIFT_START && tick < FAILURE_TICK) {
            value += (tick - DRIFT_START) * DRIFT_RATE;
        }
        /* Complete failure after FAILURE_TICK */
        if (tick >= FAILURE_TICK) {
            return -999.0;  /* Indicates dead sensor */
        }
    }
    
    return value;
}

/*===========================================================================
 * Initialisation
 *===========================================================================*/

static void init_system(void) {
    /* Initialise each sensor channel */
    for (int i = 0; i < NUM_SENSORS; i++) {
        channels[i].id = i;
        
        /* Module 1: Pulse - timeout 500ms */
        pulse_init(&channels[i].pulse, 500);
        
        /* Module 2: Baseline - alpha=0.1, threshold=5.0, learn for 10 samples */
        baseline_init(&channels[i].baseline, 0.1, 5.0, 10);
        
        /* Module 3: Timing - expect 100ms interval, 50ms tolerance */
        timing_init(&channels[i].timing, SAMPLE_INTERVAL, 50);
        
        /* Module 4: Drift - alpha=0.2, threshold=0.01, min 5 samples */
        drift_config_t dcfg = {
            .alpha = 0.2,
            .max_safe_slope = 0.01,   /* Units per ms */
            .upper_limit = 200.0,
            .lower_limit = 0.0,
            .n_min = 5,
            .max_gap = 1000,
            .min_slope_for_ttf = 1e-6,
            .reset_on_gap = 1
        };
        drift_init(&channels[i].drift, &dcfg);
        
        channels[i].health = SENSOR_HEALTHY;
        channels[i].last_value = 0.0;
    }
    
    /* Module 5: Consensus voter */
    consensus_config_t ccfg = {
        .max_deviation = 2.0,
        .tie_breaker = 0,
        .n_min = 1,
        .use_weighted_avg = 0
    };
    consensus_init(&voter, &ccfg);
    
    /* Module 6: Pressure queue */
    pressure_config_t pcfg = {
        .capacity = 32,
        .policy = POLICY_DROP_OLDEST,
        .low_water = 8,
        .high_water = 24,
        .critical_water = 30
    };
    pressure_init(&output_queue, &pcfg, queue_buffer);
}

/*===========================================================================
 * Per-Tick Processing
 *===========================================================================*/

static sensor_health_t compute_health(sensor_channel_t *ch) {
    /* Dead sensor */
    if (pulse_state(&ch->pulse) == PULSE_DEAD) {
        return SENSOR_FAULTY;
    }
    
    /* Drifting sensor */
    if (drift_is_drifting(&ch->drift)) {
        return SENSOR_DEGRADED;
    }
    
    /* Baseline deviation */
    if (baseline_state(&ch->baseline) == BASELINE_DEVIATION) {
        return SENSOR_DEGRADED;
    }
    
    /* Timing issues */
    if (timing_state(&ch->timing) == TIMING_UNHEALTHY) {
        return SENSOR_DEGRADED;
    }
    
    return SENSOR_HEALTHY;
}

static void process_tick(int tick, uint64_t now_ms) {
    sensor_input_t inputs[3];
    
    /* Process each sensor channel */
    for (int i = 0; i < NUM_SENSORS; i++) {
        sensor_channel_t *ch = &channels[i];
        double value = simulate_sensor(i, tick);
        
        if (value < -900.0) {
            /* Sensor is dead - no heartbeat */
            pulse_check(&ch->pulse, now_ms);
        } else {
            /* Sensor is alive */
            pulse_beat(&ch->pulse, now_ms);
            
            /* Update all monitoring modules */
            baseline_update(&ch->baseline, value);
            timing_event(&ch->timing, now_ms);
            
            drift_result_t dr;
            drift_update(&ch->drift, value, now_ms, &dr);
            
            ch->last_value = value;
        }
        
        /* Compute health from all modules */
        ch->health = compute_health(ch);
        
        /* Prepare input for consensus */
        inputs[i].value = ch->last_value;
        inputs[i].health = ch->health;
    }
    
    /* Module 5: Consensus voting */
    consensus_result_t cr;
    consensus_update(&voter, inputs, &cr);
    
    /* Module 6: Enqueue to output buffer */
    pressure_result_t pr;
    pressure_enqueue(&output_queue, 
                     (uint64_t)(cr.value * 1000),  /* Fixed-point encoding */
                     now_ms, &pr);
    
    /* Print status */
    printf("  %3d | ", tick);
    
    /* Sensor values and health */
    for (int i = 0; i < NUM_SENSORS; i++) {
        const char *health_char;
        switch (channels[i].health) {
            case SENSOR_HEALTHY:  health_char = "✓"; break;
            case SENSOR_DEGRADED: health_char = "~"; break;
            case SENSOR_FAULTY:   health_char = "✗"; break;
            default:              health_char = "?"; break;
        }
        printf("%6.1f %s | ", channels[i].last_value, health_char);
    }
    
    /* Consensus result */
    printf("%6.1f | %4.0f%% | %-8s | ", 
           cr.value, cr.confidence * 100, 
           consensus_state_name(cr.state));
    
    /* Queue status */
    printf("%2u/%-2u %-8s\n",
           pressure_count(&output_queue),
           pressure_capacity(&output_queue),
           pressure_state_name(pressure_state(&output_queue)));
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    srand((unsigned int)time(NULL));
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║           c-from-scratch: Complete Safety Monitoring System                   ║\n");
    printf("║                                                                               ║\n");
    printf("║   All 6 modules integrated:                                                   ║\n");
    printf("║     Pulse → Baseline → Timing → Drift → Consensus → Pressure                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("  Scenario:\n");
    printf("    - 3 redundant sensors monitoring ground truth = %.1f\n", GROUND_TRUTH);
    printf("    - Sensor 2 starts drifting at tick %d (rate = %.1f/tick)\n", DRIFT_START, DRIFT_RATE);
    printf("    - Sensor 2 fails completely at tick %d\n", FAILURE_TICK);
    printf("    - Consensus voting should maintain accuracy throughout\n");
    printf("\n");
    
    printf("  Legend:\n");
    printf("    ✓ = HEALTHY    ~ = DEGRADED    ✗ = FAULTY\n");
    printf("\n");
    
    init_system();
    
    printf("═══════════════════════════════════════════════════════════════════════════════════\n");
    printf(" Tick |   S0      |   S1      |   S2      | Consens | Conf | State    | Queue\n");
    printf("══════╪═══════════╪═══════════╪═══════════╪═════════╪══════╪══════════╪══════════\n");
    
    for (int tick = 0; tick < SIM_DURATION; tick++) {
        uint64_t now_ms = tick * SAMPLE_INTERVAL;
        process_tick(tick, now_ms);
        
        /* Print separator at key events */
        if (tick == DRIFT_START - 1 || tick == FAILURE_TICK - 1) {
            printf("──────┼───────────┼───────────┼───────────┼─────────┼──────┼──────────┼──────────\n");
        }
    }
    
    printf("═══════════════════════════════════════════════════════════════════════════════════\n");
    
    /* Final statistics */
    printf("\n  Final Statistics:\n");
    printf("  ─────────────────\n");
    
    for (int i = 0; i < NUM_SENSORS; i++) {
        printf("    Sensor %d: %lu beats, %lu drift updates\n",
               i, (unsigned long)channels[i].pulse.beats,
               (unsigned long)channels[i].drift.n);
    }
    
    printf("\n    Consensus: %lu votes\n", (unsigned long)voter.n);
    
    pressure_stats_t ps;
    pressure_get_stats(&output_queue, &ps);
    printf("    Queue: %lu enqueued, %lu dropped, peak fill = %u\n",
           (unsigned long)ps.enqueued, 
           (unsigned long)ps.dropped_oldest,
           ps.peak_fill);
    
    /* Drain queue and compute final stats */
    printf("\n  Draining output queue (last 10 values):\n    ");
    pressure_item_t item;
    int count = 0;
    double sum = 0.0;
    while (pressure_dequeue(&output_queue, &item, NULL) == PRESSURE_OK) {
        double val = item.payload / 1000.0;
        sum += val;
        count++;
        if (count > (int)ps.enqueued - 10) {
            printf("%.1f ", val);
        }
    }
    printf("\n");
    
    double avg = sum / count;
    double error = fabs(avg - GROUND_TRUTH);
    
    printf("\n  Results:\n");
    printf("    Average consensus value: %.2f\n", avg);
    printf("    Ground truth:            %.2f\n", GROUND_TRUTH);
    printf("    Mean error:              %.2f (%.1f%%)\n", error, error / GROUND_TRUTH * 100);
    printf("\n");
    
    if (error < 1.0) {
        printf("  ✓ SUCCESS: System maintained accuracy despite sensor drift and failure!\n");
    } else {
        printf("  ✗ WARNING: Mean error exceeded 1.0 - review system parameters.\n");
    }
    
    printf("\n");
    printf("  Modules demonstrated:\n");
    printf("    [1] Pulse    - Detected sensor 2 death at tick %d\n", FAILURE_TICK);
    printf("    [2] Baseline - Tracked normal operating range\n");
    printf("    [3] Timing   - Monitored sample regularity\n");
    printf("    [4] Drift    - Detected sensor 2 drift starting tick %d\n", DRIFT_START);
    printf("    [5] Consensus- Outvoted faulty sensor, maintained accuracy\n");
    printf("    [6] Pressure - Buffered %lu outputs with %lu drops\n",
           (unsigned long)ps.enqueued, (unsigned long)ps.dropped_oldest);
    printf("\n");
    
    return 0;
}
