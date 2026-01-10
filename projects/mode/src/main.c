/**
 * main.c - Mode Manager Demo
 * 
 * Demonstrates the Mode Manager (Module 7) orchestrating
 * a safety-critical system through various scenarios.
 * 
 * Copyright (c) 2026 William Murray
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include "mode.h"

/*===========================================================================
 * Demo Helpers
 *===========================================================================*/

static void print_header(const char *title) {
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════════\n");
}

static void print_result(const mode_result_t *r) {
    printf("  Mode: %-12s | Dwell: %3u | Fault: %s | ",
           mode_name(r->mode),
           r->ticks_in_mode,
           r->fault_active ? "YES" : "NO ");
    
    printf("Actions: %s%s%s%s\n",
           r->permissions.can_actuate ? "ACT " : "--- ",
           r->permissions.can_calibrate ? "CAL " : "--- ",
           r->permissions.can_log ? "LOG " : "--- ",
           r->permissions.can_communicate ? "COM" : "---");
}

static void print_input(const mode_input_t *input) {
    printf("  Input: [");
    const char *names[] = {"PUL", "BAS", "TIM", "DRI", "CON", "PRE"};
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        const char *state;
        switch (input->states[i]) {
            case HEALTH_UNKNOWN:  state = "?"; break;
            case HEALTH_LEARNING: state = "L"; break;
            case HEALTH_HEALTHY:  state = "H"; break;
            case HEALTH_DEGRADED: state = "D"; break;
            case HEALTH_FAULTY:   state = "F"; break;
            default:              state = "?"; break;
        }
        printf(" %s:%s", names[i], state);
    }
    printf(" ]");
    
    if (input->flags.approaching_upper) printf(" [↑LIMIT]");
    if (input->flags.approaching_lower) printf(" [↓LIMIT]");
    if (input->flags.low_confidence)    printf(" [LOWCONF]");
    if (input->flags.queue_critical)    printf(" [QCRIT]");
    if (input->flags.timing_unstable)   printf(" [JITTER]");
    if (input->flags.baseline_volatile) printf(" [VOLATILE]");
    
    printf("\n");
}

static void update_and_print(mode_manager_t *m, mode_input_t *input, int tick) {
    mode_result_t r;
    mode_update(m, input, &r);
    
    printf("\n  [Tick %2d] ", tick);
    if (r.transitioned) {
        printf(">>> TRANSITION!\n");
    } else {
        printf("\n");
    }
    print_input(input);
    print_result(&r);
}

/*===========================================================================
 * Demo 1: Normal Startup Sequence
 *===========================================================================*/

static void demo_normal_startup(void) {
    print_header("Demo 1: Normal Startup Sequence (INIT → STARTUP → OPERATIONAL)");
    
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 5;  /* Speed up for demo */
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    
    printf("\n  Initial state: %s\n", mode_name(mode_get(&m)));
    
    /* Tick 0: All unknown → stay in INIT */
    input.timestamp = 0;
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_UNKNOWN;
    }
    update_and_print(&m, &input, 0);
    
    /* Tick 1: All learning → transition to STARTUP */
    input.timestamp = 100;
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_LEARNING;
    }
    update_and_print(&m, &input, 1);
    
    /* Ticks 2-5: Stay in STARTUP (learning) */
    for (int tick = 2; tick <= 5; tick++) {
        input.timestamp = tick * 100;
        update_and_print(&m, &input, tick);
    }
    
    /* Tick 6: All healthy → transition to OPERATIONAL */
    input.timestamp = 600;
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    update_and_print(&m, &input, 6);
    
    /* Tick 7-8: Stay in OPERATIONAL */
    for (int tick = 7; tick <= 8; tick++) {
        input.timestamp = tick * 100;
        update_and_print(&m, &input, tick);
    }
    
    printf("\n  ✓ Normal startup complete: INIT → STARTUP → OPERATIONAL\n");
}

/*===========================================================================
 * Demo 2: Degradation from Value Flags
 *===========================================================================*/

static void demo_value_flags(void) {
    print_header("Demo 2: Proactive Degradation (approaching_upper flag)");
    
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    cfg.min_dwell_degraded = 3;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    
    /* Fast-forward to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    mode_result_t r;
    mode_update(&m, &input, &r);  /* INIT → STARTUP */
    mode_update(&m, &input, &r);  /* STARTUP dwell */
    mode_update(&m, &input, &r);  /* STARTUP → OPERATIONAL */
    
    printf("\n  Starting in OPERATIONAL mode\n");
    
    /* Tick 0: All healthy, no flags */
    input.timestamp = 0;
    update_and_print(&m, &input, 0);
    
    /* Tick 1: Drift reports approaching_upper */
    input.timestamp = 100;
    input.flags.approaching_upper = 1;
    printf("\n  >>> Drift module sets 'approaching_upper' flag\n");
    update_and_print(&m, &input, 1);
    
    /* Tick 2: Still in DEGRADED with flag */
    input.timestamp = 200;
    update_and_print(&m, &input, 2);
    
    /* Tick 3: Flag clears but dwell not met */
    input.timestamp = 300;
    input.flags.approaching_upper = 0;
    printf("\n  >>> Flag clears, but min_dwell not met yet\n");
    update_and_print(&m, &input, 3);
    
    /* Tick 4: Dwell met, recover to OPERATIONAL */
    input.timestamp = 400;
    update_and_print(&m, &input, 4);
    
    printf("\n  ✓ Proactive safety: Value flag triggered DEGRADED BEFORE actual fault\n");
}

/*===========================================================================
 * Demo 3: Emergency from Fault
 *===========================================================================*/

static void demo_emergency(void) {
    print_header("Demo 3: Emergency Fault (OPERATIONAL → EMERGENCY)");
    
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    
    /* Fast-forward to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    mode_result_t r;
    mode_update(&m, &input, &r);
    mode_update(&m, &input, &r);
    mode_update(&m, &input, &r);
    
    printf("\n  Starting in OPERATIONAL mode\n");
    
    /* Tick 0: Normal operation */
    input.timestamp = 0;
    update_and_print(&m, &input, 0);
    
    /* Tick 1: Pulse goes FAULTY (sensor died) */
    input.timestamp = 100;
    input.states[MOD_PULSE] = HEALTH_FAULTY;
    printf("\n  >>> Pulse module reports FAULTY (sensor died!)\n");
    update_and_print(&m, &input, 1);
    
    /* Tick 2: EMERGENCY is sticky */
    input.timestamp = 200;
    input.states[MOD_PULSE] = HEALTH_HEALTHY;  /* Even if it "recovers" */
    printf("\n  >>> Pulse reports HEALTHY again, but EMERGENCY is sticky\n");
    update_and_print(&m, &input, 2);
    
    /* Tick 3: Still stuck */
    input.timestamp = 300;
    update_and_print(&m, &input, 3);
    
    /* Manual reset required */
    printf("\n  >>> Manual reset triggered\n");
    mode_reset(&m);
    input.timestamp = 400;
    update_and_print(&m, &input, 4);
    
    printf("\n  ✓ Fault stickiness proven: EMERGENCY requires explicit reset\n");
}

/*===========================================================================
 * Demo 4: Transition History (Audit Log)
 *===========================================================================*/

static void demo_audit_log(void) {
    print_header("Demo 4: Transition History (Audit Log)");
    
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    cfg.min_dwell_degraded = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    mode_result_t r;
    
    /* Generate some transitions */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_LEARNING;
    }
    input.timestamp = 100;
    mode_update(&m, &input, &r);  /* INIT → STARTUP */
    
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    input.timestamp = 200;
    mode_update(&m, &input, &r);  /* dwell */
    input.timestamp = 300;
    mode_update(&m, &input, &r);  /* STARTUP → OPERATIONAL */
    
    input.states[MOD_CONSENSUS] = HEALTH_DEGRADED;
    input.timestamp = 400;
    mode_update(&m, &input, &r);  /* OPERATIONAL → DEGRADED */
    
    input.states[MOD_CONSENSUS] = HEALTH_HEALTHY;
    input.timestamp = 500;
    mode_update(&m, &input, &r);  /* dwell */
    input.timestamp = 600;
    mode_update(&m, &input, &r);  /* DEGRADED → OPERATIONAL */
    
    input.states[MOD_DRIFT] = HEALTH_FAULTY;
    input.timestamp = 700;
    mode_update(&m, &input, &r);  /* OPERATIONAL → EMERGENCY */
    
    mode_reset(&m);  /* EMERGENCY → INIT */
    
    /* Print history */
    printf("\n  Transition History:\n");
    printf("  ───────────────────────────────────────────────────────────\n");
    printf("  %-6s | %-12s → %-12s | Trigger\n", "Time", "From", "To");
    printf("  ───────────────────────────────────────────────────────────\n");
    
    mode_transition_t history[MODE_HISTORY_SIZE];
    int count = mode_get_history(&m, history, MODE_HISTORY_SIZE);
    
    for (int i = 0; i < count; i++) {
        mode_transition_t *t = &history[i];
        printf("  %6lu | %-12s → %-12s | ",
               (unsigned long)t->timestamp,
               mode_name(t->from_mode),
               mode_name(t->to_mode));
        
        if (t->trigger_mask & TRIGGER_PULSE)     printf("PULSE ");
        if (t->trigger_mask & TRIGGER_BASELINE)  printf("BASELINE ");
        if (t->trigger_mask & TRIGGER_TIMING)    printf("TIMING ");
        if (t->trigger_mask & TRIGGER_DRIFT)     printf("DRIFT ");
        if (t->trigger_mask & TRIGGER_CONSENSUS) printf("CONSENSUS ");
        if (t->trigger_mask & TRIGGER_PRESSURE)  printf("PRESSURE ");
        if (t->trigger_mask & TRIGGER_FLAGS)     printf("FLAGS ");
        if (t->trigger_mask & TRIGGER_RESET)     printf("RESET ");
        if (t->trigger_mask == 0)                printf("(automatic)");
        printf("\n");
    }
    
    printf("\n  Total transitions: %u\n", m.total_transitions);
    printf("  Emergency count:   %u\n", m.emergency_count);
    
    printf("\n  ✓ Full audit trail maintained (CONTRACT-8: Auditability)\n");
}

/*===========================================================================
 * Demo 5: Permissions Check
 *===========================================================================*/

static void demo_permissions(void) {
    print_header("Demo 5: Mode Permissions (What's Allowed?)");
    
    printf("\n  Mode Permissions Table:\n");
    printf("  ─────────────────────────────────────────────────────────\n");
    printf("  %-12s | ACT | CAL | LOG | COM\n", "Mode");
    printf("  ─────────────────────────────────────────────────────────\n");
    
    for (int i = 0; i < MODE_COUNT; i++) {
        mode_permissions_t p = MODE_PERMISSIONS[i];
        printf("  %-12s |  %c  |  %c  |  %c  |  %c\n",
               mode_name(i),
               p.can_actuate ? 'Y' : '-',
               p.can_calibrate ? 'Y' : '-',
               p.can_log ? 'Y' : '-',
               p.can_communicate ? 'Y' : '-');
    }
    
    printf("\n  Legend:\n");
    printf("    ACT = Actuation (thrusters, motors)\n");
    printf("    CAL = Calibration routines\n");
    printf("    LOG = Data logging\n");
    printf("    COM = Communication/telemetry\n");
    
    printf("\n  ✓ Modes constrain actions — the key safety mechanism\n");
}

/*===========================================================================
 * Demo 6: Multiple Flags Escalation
 *===========================================================================*/

static void demo_flag_escalation(void) {
    print_header("Demo 6: Multiple Warning Flags");
    
    mode_manager_t m;
    mode_config_t cfg = MODE_DEFAULT_CONFIG;
    cfg.min_dwell_startup = 1;
    mode_init(&m, &cfg);
    
    mode_input_t input = {0};
    
    /* Fast-forward to OPERATIONAL */
    for (int i = 0; i < MODE_MODULE_COUNT; i++) {
        input.states[i] = HEALTH_HEALTHY;
    }
    mode_result_t r;
    mode_update(&m, &input, &r);
    mode_update(&m, &input, &r);
    mode_update(&m, &input, &r);
    
    printf("\n  Starting in OPERATIONAL mode\n");
    
    /* Tick 0: Normal */
    input.timestamp = 0;
    update_and_print(&m, &input, 0);
    
    /* Tick 1: Multiple flags set simultaneously */
    input.timestamp = 100;
    input.flags.approaching_upper = 1;
    input.flags.low_confidence = 1;
    input.flags.queue_critical = 1;
    printf("\n  >>> Multiple warnings: approaching_upper + low_confidence + queue_critical\n");
    update_and_print(&m, &input, 1);
    
    printf("\n  Note: All states still HEALTHY, but flags triggered DEGRADED\n");
    printf("  This is proactive safety — act before failure.\n");
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║        Module 7: Mode Manager — The Captain                   ║\n");
    printf("║                                                               ║\n");
    printf("║   \"While Modules 1-6 answer 'What's happening?',              ║\n");
    printf("║    Module 7 answers 'What do we DO about it?'\"                ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    demo_normal_startup();
    demo_value_flags();
    demo_emergency();
    demo_audit_log();
    demo_permissions();
    demo_flag_escalation();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Demo Complete\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("  Contracts demonstrated:\n");
    printf("    CONTRACT-1: Unambiguous state (one mode at a time)\n");
    printf("    CONTRACT-2: Safe entry (OPERATIONAL requires all healthy)\n");
    printf("    CONTRACT-3: Fault stickiness (EMERGENCY requires reset)\n");
    printf("    CONTRACT-4: No skip (INIT → STARTUP → OPERATIONAL)\n");
    printf("    CONTRACT-5: Bounded latency (fault → EMERGENCY in 1 cycle)\n");
    printf("    CONTRACT-6: Deterministic (same inputs → same mode)\n");
    printf("    CONTRACT-7: Proactive safety (flags trigger DEGRADED)\n");
    printf("    CONTRACT-8: Auditability (all transitions logged)\n");
    printf("\n");
    printf("  The Mode Manager is the \"Captain\" of the safety-critical ship.\n");
    printf("  Sensors report. The Captain decides.\n");
    printf("\n");
    
    return 0;
}
