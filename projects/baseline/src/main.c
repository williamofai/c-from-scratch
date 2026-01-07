/**
 * main.c - Example usage of baseline normality monitor
 * 
 * This demonstrates:
 *   1. Basic usage of the baseline API
 *   2. Learning phase (warm-up)
 *   3. Stable operation with normal data
 *   4. Deviation detection with anomalies
 *   5. Recovery after anomaly passes
 *   6. Spike resistance (CONTRACT-4)
 * 
 * Pulse tells us the heartbeat exists.
 * Baseline tells us if the heart rate is pathological.
 * 
 * Copyright (c) 2025 William Murray
 * MIT License - https://github.com/williamofai/c-from-scratch
 */

#include <stdio.h>
#include <stdlib.h>
#include "baseline.h"

/*---------------------------------------------------------------------------
 * Demo: Normal Operation
 *---------------------------------------------------------------------------*/

static void demo_normal_operation(void)
{
    printf("\n");
    printf("=======================================================\n");
    printf("Demo 1: Normal Operation\n");
    printf("=======================================================\n");
    printf("Feeding stable values around 100.0 with small noise.\n");
    printf("Expected: LEARNING → STABLE, no deviations.\n\n");
    
    base_fsm_t monitor;
    base_init(&monitor, &BASE_DEFAULT_CONFIG);
    
    /* Stable values with small noise */
    double values[] = {
        100.0, 100.5, 99.5, 100.2, 99.8,
        100.1, 99.9, 100.3, 99.7, 100.0,
        100.2, 99.8, 100.1, 99.9, 100.0,
        100.0, 100.1, 99.9, 100.2, 99.8,
        100.0, 100.1, 99.9, 100.0, 100.0
    };
    int n = sizeof(values) / sizeof(values[0]);
    
    printf("%4s  %8s  %8s  %8s  %8s  %10s\n",
           "i", "x", "mu", "sigma", "z", "state");
    printf("----  --------  --------  --------  --------  ----------\n");
    
    for (int i = 0; i < n; i++) {
        base_result_t r = base_step(&monitor, values[i]);
        printf("%4d  %8.3f  %8.4f  %8.4f  %8.4f  %10s\n",
               i + 1, values[i], monitor.mu, monitor.sigma, r.z,
               base_state_name(r.state));
    }
    
    printf("\nFinal: n=%u, ready=%s, faulted=%s\n",
           monitor.n,
           base_ready(&monitor) ? "yes" : "no",
           base_faulted(&monitor) ? "yes" : "no");
}

/*---------------------------------------------------------------------------
 * Demo: Anomaly Detection
 *---------------------------------------------------------------------------*/

static void demo_anomaly_detection(void)
{
    printf("\n");
    printf("=======================================================\n");
    printf("Demo 2: Anomaly Detection\n");
    printf("=======================================================\n");
    printf("Learn a tight baseline at 100.0, then inject spike.\n");
    printf("Expected: STABLE → DEVIATION → STABLE (recovery).\n\n");
    
    base_fsm_t monitor;
    
    /* Use config with enough learning to converge */
    base_config_t cfg = {
        .alpha   = 0.1,
        .epsilon = 1e-9,
        .k       = 3.0,
        .n_min   = 30
    };
    base_init(&monitor, &cfg);
    
    /* Learning phase: many observations to fully converge */
    printf("--- Learning phase (100 observations at 100.0 ± 0.5) ---\n");
    for (int i = 0; i < 100; i++) {
        double x = 100.0 + (i % 2 == 0 ? 0.5 : -0.5);
        base_step(&monitor, x);
    }
    printf("Learned: mu=%.4f, sigma=%.4f, state=%s\n\n",
           monitor.mu, monitor.sigma, base_state_name(monitor.state));
    
    /* Now sigma is small (~0.5) and mu is close to 100 */
    /* A spike of 110 should be ~20 sigma = definitely deviation */
    double spike = 115.0;
    printf("--- Injecting anomaly: x = %.1f ---\n", spike);
    printf("This is %.1f sigma above the mean.\n", 
           (spike - monitor.mu) / monitor.sigma);
    
    base_result_t r = base_step(&monitor, spike);
    printf("Result: z=%.2f, state=%s, is_deviation=%d\n",
           r.z, base_state_name(r.state), r.is_deviation);
    if (r.state == BASE_DEVIATION) {
        printf("*** DEVIATION detected: z=%.2f > k=%.1f ***\n\n",
               r.z, monitor.cfg.k);
    }
    
    /* Show spike resistance: how much did mu shift? */
    /* Reconstruct mu_old from: mu_new = alpha*x + (1-alpha)*mu_old
     * Therefore: mu_old = (mu_new - alpha*x) / (1-alpha)
     * Or equivalently: mu_new - mu_old = alpha * (x - mu_old) = alpha * deviation
     * So: mu_old = mu_new - alpha * deviation */
    double mu_after = monitor.mu;
    double mu_before = mu_after - monitor.cfg.alpha * r.deviation;
    printf("--- Spike Resistance (CONTRACT-4) ---\n");
    printf("Spike deviation = %.2f\n", r.deviation);
    printf("Max allowed shift: alpha * |deviation| = %.2f\n",
           monitor.cfg.alpha * (r.deviation > 0 ? r.deviation : -r.deviation));
    printf("Actual shift: %.4f → %.4f (Δ = %.4f)\n",
           mu_before, mu_after, mu_after - mu_before);
    printf("Mean bounded — no catastrophic corruption!\n\n");
    
    /* Recovery: return to normal values */
    printf("--- Recovery phase ---\n");
    for (int i = 0; i < 15; i++) {
        double x = 100.0 + (i % 2 == 0 ? 0.5 : -0.5);
        r = base_step(&monitor, x);
        printf("x=%.1f: z=%.4f, state=%s\n",
               x, r.z, base_state_name(r.state));
        if (r.state == BASE_STABLE) {
            printf("*** Recovered to STABLE ***\n");
            break;
        }
    }
}

/*---------------------------------------------------------------------------
 * Demo: Sustained Deviation
 *---------------------------------------------------------------------------*/

static void demo_sustained_deviation(void)
{
    printf("\n");
    printf("=======================================================\n");
    printf("Demo 3: Sustained Deviation (Level Shift)\n");
    printf("=======================================================\n");
    printf("Learn tight baseline at 100, then shift to 120.\n");
    printf("Expected: DEVIATION → eventually STABLE as baseline adapts.\n\n");
    
    base_fsm_t monitor;
    
    base_config_t cfg = {
        .alpha   = 0.1,
        .epsilon = 1e-9,
        .k       = 3.0,
        .n_min   = 30
    };
    base_init(&monitor, &cfg);
    
    /* Learning phase: many observations to converge */
    for (int i = 0; i < 100; i++) {
        double x = 100.0 + (i % 2 == 0 ? 0.5 : -0.5);
        base_step(&monitor, x);
    }
    printf("Learned baseline: mu=%.4f, sigma=%.4f\n\n", 
           monitor.mu, monitor.sigma);
    
    /* Level shift to 120 — large enough to trigger deviation */
    double new_level = 120.0;
    printf("--- Shifting to %.1f (%.1fσ above baseline) ---\n",
           new_level, (new_level - monitor.mu) / monitor.sigma);
    printf("%4s  %8s  %8s  %8s  %10s\n",
           "i", "x", "mu", "z", "state");
    printf("----  --------  --------  --------  ----------\n");
    
    int first_deviation = -1;
    int first_stable = -1;
    for (int i = 0; i < 50; i++) {
        base_result_t r = base_step(&monitor, new_level);
        
        /* Track state transitions */
        if (first_deviation < 0 && r.state == BASE_DEVIATION) {
            first_deviation = i + 1;
        }
        if (first_deviation > 0 && first_stable < 0 && r.state == BASE_STABLE) {
            first_stable = i + 1;
        }
        
        /* Print key steps */
        if (i < 5 || r.state != BASE_DEVIATION || i == 49 || first_stable == i + 1) {
            printf("%4d  %8.1f  %8.4f  %8.4f  %10s\n",
                   i + 1, new_level, monitor.mu, r.z, base_state_name(r.state));
        } else if (i == 5) {
            printf("  ...  (in DEVIATION, adapting) ...\n");
        }
        
        if (first_stable > 0) {
            break;
        }
    }
    
    if (first_deviation > 0) {
        printf("\n*** DEVIATION detected at step %d ***\n", first_deviation);
    }
    if (first_stable > 0) {
        printf("*** Baseline adapted at step %d — new normal established ***\n", first_stable);
        printf("\nThis demonstrates CONTRACT-2 (Sensitivity):\n");
        printf("Adaptation took %d steps (effective window ≈ 2/α = %.0f).\n",
               first_stable, 2.0 / monitor.cfg.alpha);
    }
}

/*---------------------------------------------------------------------------
 * Demo: Fault Handling
 *---------------------------------------------------------------------------*/

static void demo_fault_handling(void)
{
    printf("\n");
    printf("=======================================================\n");
    printf("Demo 4: Fault Handling (NaN Input)\n");
    printf("=======================================================\n");
    printf("Inject NaN — expect fault_fp, sticky until reset.\n\n");
    
    base_fsm_t monitor;
    base_init(&monitor, &BASE_DEFAULT_CONFIG);
    
    /* Some normal observations */
    for (int i = 0; i < 5; i++) {
        base_step(&monitor, 100.0);
    }
    printf("Before fault: n=%u, state=%s, faulted=%s\n",
           monitor.n, base_state_name(monitor.state),
           base_faulted(&monitor) ? "yes" : "no");
    
    /* Inject NaN */
    printf("\n--- Injecting NaN ---\n");
    double nan_val = 0.0 / 0.0;  /* Generate NaN */
    base_result_t r = base_step(&monitor, nan_val);
    printf("After NaN: n=%u, state=%s, faulted=%s\n",
           monitor.n, base_state_name(r.state),
           base_faulted(&monitor) ? "yes" : "no");
    printf("Note: n unchanged (faulted input does not increment n)\n");
    
    /* Try more observations — fault is sticky */
    printf("\n--- Attempting recovery (fault is sticky) ---\n");
    for (int i = 0; i < 3; i++) {
        r = base_step(&monitor, 100.0);
        printf("x=100.0: state=%s, faulted=%s\n",
               base_state_name(r.state),
               base_faulted(&monitor) ? "yes" : "no");
    }
    printf("Fault persists — must call base_reset() to clear.\n");
    
    /* Reset */
    printf("\n--- Calling base_reset() ---\n");
    base_reset(&monitor);
    printf("After reset: n=%u, state=%s, faulted=%s\n",
           monitor.n, base_state_name(monitor.state),
           base_faulted(&monitor) ? "yes" : "no");
}

/*---------------------------------------------------------------------------
 * Main
 *---------------------------------------------------------------------------*/

int main(void)
{
    printf("baseline - Statistical Normality Monitor Demo\n");
    printf("=============================================\n");
    printf("\n");
    printf("Module 1 (Pulse) proved existence in time.\n");
    printf("Module 2 (Baseline) proves normality in value.\n");
    printf("\n");
    printf("Default configuration:\n");
    printf("  alpha   = %.2f  (effective window ≈ %.0f observations)\n",
           BASE_DEFAULT_CONFIG.alpha, 2.0 / BASE_DEFAULT_CONFIG.alpha);
    printf("  epsilon = %.0e  (variance floor)\n", BASE_DEFAULT_CONFIG.epsilon);
    printf("  k       = %.1f  (deviation threshold, sigma units)\n",
           BASE_DEFAULT_CONFIG.k);
    printf("  n_min   = %u   (learning period)\n", BASE_DEFAULT_CONFIG.n_min);
    
    demo_normal_operation();
    demo_anomaly_detection();
    demo_sustained_deviation();
    demo_fault_handling();
    
    printf("\n");
    printf("=======================================================\n");
    printf("Demo Complete\n");
    printf("=======================================================\n");
    printf("\n");
    printf("Key insights demonstrated:\n");
    printf("  1. LEARNING → STABLE transition after n_min observations\n");
    printf("  2. Deviation detection when z > k\n");
    printf("  3. Spike resistance: single outlier bounded by alpha*M\n");
    printf("  4. Adaptation to sustained level shifts\n");
    printf("  5. Sticky faults, cleared only by reset\n");
    printf("\n");
    printf("Next: Compose Pulse + Baseline for timing anomaly detection.\n");
    printf("  Pulse outputs inter-arrival times Δt\n");
    printf("  Baseline monitors: is this Δt normal?\n");
    
    return 0;
}
