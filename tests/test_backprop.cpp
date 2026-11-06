#include "modules/backprop/network_types.h"
#include "modules/backprop/backprop_engine.h"

#include <iostream>
#include <fstream>
#include <cmath>
#include <string>
#include <vector>

using namespace workbench;
using namespace workbench::backprop;

static int g_bp_passed = 0;
static int g_bp_failed = 0;

#define BP_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            g_bp_passed++; \
        } else { \
            g_bp_failed++; \
            std::cerr << "  [FAIL] Line " << __LINE__ << ": " << (msg) << "\n"; \
        } \
    } while (0)

// 1. Initial forward propagation & initial loss
void test_initial_forward_pass() {
    std::cout << "[TEST GROUP 1] Initial Forward Propagation & Loss...\n";

    const double x = 1.0;
    const double y = 2.0;
    NetworkParameters params;
    params.w = 0.5;
    params.b = 0.0;
    params.v = 1.0;
    params.c = 0.0;

    const ForwardOutput out = forward_pass(x, y, params);

    // Expected z = 0.5 * 1 + 0 = 0.5
    BP_ASSERT(std::abs(out.z - 0.5) < 1e-12, "Pre-activation z is exactly 0.5");

    // Expected h = tanh(0.5) ~ 0.4621171572600098
    const double expected_h = std::tanh(0.5);
    BP_ASSERT(std::abs(out.h - expected_h) < 1e-12, "Hidden activation h matches tanh(0.5)");

    // Expected y_pred = 1.0 * h + 0.0 = h
    BP_ASSERT(std::abs(out.y_pred - expected_h) < 1e-12, "y_pred matches v*h + c");

    // Expected initial loss = 1.182541818997
    const double expected_loss = 1.182541818997;
    BP_ASSERT(std::abs(out.loss - expected_loss) < 1e-9, "Initial loss matches 1.182541818997");
}

// 2. Analytical gradients against exact expected values
void test_analytical_gradients() {
    std::cout << "[TEST GROUP 2] Analytical Gradients via Chain Rule...\n";

    const double x = 1.0;
    const double y = 2.0;
    NetworkParameters params;
    params.w = 0.5;
    params.b = 0.0;
    params.v = 1.0;
    params.c = 0.0;

    const Gradients grads = compute_analytical_gradients(x, y, params);

    // Expected analytical values:
    // dJ/dw = -1.209464475240
    // dJ/db = -1.209464475240
    // dJ/dv = -0.710682047486
    // dJ/dc = -1.537882842740
    BP_ASSERT(std::abs(grads.dw - (-1.209464475240)) < 1e-9, "dJ/dw matches -1.209464475240");
    BP_ASSERT(std::abs(grads.db - (-1.209464475240)) < 1e-9, "dJ/db matches -1.209464475240");
    BP_ASSERT(std::abs(grads.dv - (-0.710682047486)) < 1e-9, "dJ/dv matches -0.710682047486");
    BP_ASSERT(std::abs(grads.dc - (-1.537882842740)) < 1e-9, "dJ/dc matches -1.537882842740");
}

// 3. Simultaneous parameter update values
void test_simultaneous_parameter_update() {
    std::cout << "[TEST GROUP 3] Simultaneous Parameter Updates...\n";

    const double x = 1.0;
    const double y = 2.0;
    const double lr = 0.1;
    NetworkParameters params;
    params.w = 0.5;
    params.b = 0.0;
    params.v = 1.0;
    params.c = 0.0;

    const Gradients grads = compute_analytical_gradients(x, y, params);
    const NetworkParameters updated = update_parameters_simultaneous(params, grads, lr);

    // Expected updated parameters:
    // w = 0.620946447524
    // b = 0.120946447524
    // v = 1.071068204749
    // c = 0.153788284274
    BP_ASSERT(std::abs(updated.w - 0.620946447524) < 1e-9, "Updated w matches 0.620946447524");
    BP_ASSERT(std::abs(updated.b - 0.120946447524) < 1e-9, "Updated b matches 0.120946447524");
    BP_ASSERT(std::abs(updated.v - 1.071068204749) < 1e-9, "Updated v matches 1.071068204749");
    BP_ASSERT(std::abs(updated.c - 0.153788284274) < 1e-9, "Updated c matches 0.153788284274");
}

// 4. Dedicated test: Confirm all four gradients use original parameter values before updating
void test_original_parameters_enforcement() {
    std::cout << "[TEST GROUP 4] Dedicated Test: Gradients Use Original Parameters...\n";

    const double x = 1.0;
    const double y = 2.0;
    NetworkParameters params_orig;
    params_orig.w = 0.5;
    params_orig.b = 0.0;
    params_orig.v = 1.0;
    params_orig.c = 0.0;

    // Gradients calculated with original parameters
    const Gradients grads_correct = compute_analytical_gradients(x, y, params_orig);

    // If an implementation improperly mutated w in place before calculating b, v, or c:
    NetworkParameters params_improper = params_orig;
    params_improper.w = 0.620946447524; // Mutated in place

    const Gradients grads_improper = compute_analytical_gradients(x, y, params_improper);

    // Verify that mutating w in place produces DIFFERENT gradients for b, v, and c
    BP_ASSERT(std::abs(grads_improper.db - grads_correct.db) > 1e-4,
              "In-place parameter mutation alters dJ/db, confirming original parameters must be used");
    BP_ASSERT(std::abs(grads_improper.dv - grads_correct.dv) > 1e-4,
              "In-place parameter mutation alters dJ/dv, confirming original parameters must be used");
    BP_ASSERT(std::abs(grads_improper.dc - grads_correct.dc) > 1e-4,
              "In-place parameter mutation alters dJ/dc, confirming original parameters must be used");

    // Re-verify that compute_analytical_gradients leaves the original params struct unmodified
    BP_ASSERT(params_orig.w == 0.5 && params_orig.b == 0.0 && params_orig.v == 1.0 && params_orig.c == 0.0,
              "Original parameter struct is strictly constant and unmutated during gradient computation");
}

// 5. Central finite-difference verification
void test_finite_difference_verification() {
    std::cout << "[TEST GROUP 5] Central Finite Difference Verification (eps = 1e-6)...\n";

    const double x = 1.0;
    const double y = 2.0;
    const double epsilon = 1.0e-6;
    const double tolerance = 1.0e-5;

    NetworkParameters params;
    params.w = 0.5;
    params.b = 0.0;
    params.v = 1.0;
    params.c = 0.0;

    const Gradients num_grads = compute_numerical_gradients(x, y, params, epsilon);

    // Expected approximate numerical gradients:
    // w: -1.209464475349
    // b: -1.209464475349
    // v: -0.710682047433
    // c: -1.537882842628
    BP_ASSERT(std::abs(num_grads.dw - (-1.209464475349)) < 1e-8, "Numerical dw matches -1.209464475349");
    BP_ASSERT(std::abs(num_grads.db - (-1.209464475349)) < 1e-8, "Numerical db matches -1.209464475349");
    BP_ASSERT(std::abs(num_grads.dv - (-0.710682047433)) < 1e-8, "Numerical dv matches -0.710682047433");
    BP_ASSERT(std::abs(num_grads.dc - (-1.537882842628)) < 1e-8, "Numerical dc matches -1.537882842628");

    // Verification check: max error <= 1e-5
    const GradientVerificationResult verif = verify_gradients(x, y, params, epsilon, tolerance);
    BP_ASSERT(verif.passed, "verify_gradients passes within tolerance 1e-5");
    BP_ASSERT(verif.max_absolute_error <= tolerance, "Max absolute difference between analytical and numerical <= 1e-5");

    // Verify individual errors are within tolerance
    BP_ASSERT(verif.absolute_errors.dw <= tolerance, "dw error <= 1e-5");
    BP_ASSERT(verif.absolute_errors.db <= tolerance, "db error <= 1e-5");
    BP_ASSERT(verif.absolute_errors.dv <= tolerance, "dv error <= 1e-5");
    BP_ASSERT(verif.absolute_errors.dc <= tolerance, "dc error <= 1e-5");
}

// 6. Training for 100 steps & overall loss reduction
void test_training_experiment() {
    std::cout << "[TEST GROUP 6] 100-Step Gradient Descent Training Experiment...\n";

    const double x = 1.0;
    const double y = 2.0;
    const double lr = 0.1;
    const size_t steps = 100;

    NetworkParameters initial_params;
    initial_params.w = 0.5;
    initial_params.b = 0.0;
    initial_params.v = 1.0;
    initial_params.c = 0.0;

    const std::vector<TrainingStepRecord> history = train_network(x, y, initial_params, lr, steps);

    BP_ASSERT(history.size() == 100, "History contains exactly 100 step records");
    BP_ASSERT(history.front().step == 1, "First record has step == 1");
    BP_ASSERT(history.back().step == 100, "Last record has step == 100");

    bool all_finite = true;
    for (const auto& rec : history) {
        if (!std::isfinite(rec.loss) || !std::isfinite(rec.y_pred) ||
            !std::isfinite(rec.parameters.w) || !std::isfinite(rec.parameters.b) ||
            !std::isfinite(rec.parameters.v) || !std::isfinite(rec.parameters.c)) {
            all_finite = false;
            break;
        }
    }
    BP_ASSERT(all_finite, "All training loss and parameter values are strictly finite");

    // Initial loss at step 1
    const double initial_loss = history.front().loss;
    BP_ASSERT(std::abs(initial_loss - 1.182541818997) < 1e-9, "Initial loss at step 1 matches 1.182541818997");

    // Overall loss reduction requirement: final loss significantly lower than initial loss
    const double final_loss = history.back().loss;
    BP_ASSERT(final_loss < initial_loss, "Final loss is less than initial loss (overall reduction achieved)");
    BP_ASSERT(final_loss < 0.05 * initial_loss, "Final loss is reduced by over 95% from initial loss");
}

// 7. CSV export validation & error handling
void test_csv_export() {
    std::cout << "[TEST GROUP 7] CSV Export & Error Handling...\n";

    const double x = 1.0;
    const double y = 2.0;
    const double lr = 0.1;
    const size_t steps = 100;

    NetworkParameters initial_params;
    const std::vector<TrainingStepRecord> history = train_network(x, y, initial_params, lr, steps);

    const std::string csv_path = "results/backprop/loss_history.csv";
    const auto export_status = export_loss_history_csv(csv_path, history);
    BP_ASSERT(export_status.is_ok(), "export_loss_history_csv succeeds");

    std::ifstream in(csv_path);
    BP_ASSERT(in.is_open(), "Exported CSV file is openable");

    std::string header;
    std::getline(in, header);
    BP_ASSERT(header == "step,loss,y_pred,w,b,v,c,dw,db,dv,dc",
              "CSV header matches expected columns");

    size_t data_row_count = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            data_row_count++;
        }
    }
    BP_ASSERT(data_row_count == 100, "CSV file contains exactly 100 training step data rows");
    in.close();

    // Test error handling for invalid/unwritable path
    const auto bad_status = export_loss_history_csv("Z:/invalid_drive/loss_history.csv", history);
    BP_ASSERT(bad_status.is_error(), "Export to invalid path returns error status");
    BP_ASSERT(!bad_status.error().message.empty(), "Error message is non-empty and actionable");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  UNIFIED ENGINEERING WORKBENCH TESTS  \n";
    std::cout << "  Stage 8: Backpropagation (Task F)    \n";
    std::cout << "========================================\n\n";

    test_initial_forward_pass();
    test_analytical_gradients();
    test_simultaneous_parameter_update();
    test_original_parameters_enforcement();
    test_finite_difference_verification();
    test_training_experiment();
    test_csv_export();

    std::cout << "\n========================================\n";
    std::cout << "  BACKPROP TESTS SUMMARY: " << g_bp_passed << " PASSED, " << g_bp_failed << " FAILED\n";
    std::cout << "========================================\n";

    return (g_bp_failed == 0) ? 0 : 1;
}
