#pragma once

#include "core/result.h"
#include "modules/backprop/network_types.h"

#include <vector>
#include <string>

namespace workbench::backprop {

/// @brief Comparison summary between analytical and central finite-difference numerical gradients.
struct GradientVerificationResult {
    bool passed = false;
    double max_absolute_error = 0.0;
    Gradients analytical;
    Gradients numerical;
    Gradients absolute_errors;
};

/// @brief Evaluates the forward pass and quadratic loss for a sample (x, y).
ForwardOutput forward_pass(double x, double y, const NetworkParameters& params);

/// @brief Computes analytical gradients dJ/dw, dJ/db, dJ/dv, dJ/dc using the chain rule.
/// @note All gradients are computed using the supplied parameter values without modification.
Gradients compute_analytical_gradients(double x, double y, const NetworkParameters& params);

/// @brief Computes numerical gradients using central finite differences with perturbation epsilon.
Gradients compute_numerical_gradients(double x, double y, const NetworkParameters& params, double epsilon = 1.0e-6);

/// @brief Performs a simultaneous parameter update: theta_new = theta_old - learning_rate * grad.
NetworkParameters update_parameters_simultaneous(
    const NetworkParameters& params,
    const Gradients& grads,
    double learning_rate
);

/// @brief Compares analytical gradients against central finite differences, ensuring max error <= tolerance.
GradientVerificationResult verify_gradients(
    double x,
    double y,
    const NetworkParameters& params,
    double epsilon = 1.0e-6,
    double tolerance = 1.0e-5
);

/// @brief Executes gradient descent for a given number of steps, recording loss and parameters at each step.
/// @param x Input sample.
/// @param y Target sample.
/// @param initial_params Initial network parameters.
/// @param learning_rate Gradient descent step size (eta).
/// @param steps Total number of training steps (e.g., 100).
/// @return Vector of exactly 'steps' training records (1-indexed).
std::vector<TrainingStepRecord> train_network(
    double x,
    double y,
    NetworkParameters initial_params,
    double learning_rate,
    size_t steps
);

/// @brief Exports the training history records to a CSV file.
core::Status export_loss_history_csv(
    const std::string& filepath,
    const std::vector<TrainingStepRecord>& history
);

} // namespace workbench::backprop
