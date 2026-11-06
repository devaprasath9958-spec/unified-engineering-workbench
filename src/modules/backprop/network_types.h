#pragma once

#include <cstddef>

namespace workbench::backprop {

/// @brief Trainable weights and biases for the single-sample regression network.
struct NetworkParameters {
    double w = 0.5; ///< Hidden weight
    double b = 0.0; ///< Hidden bias
    double v = 1.0; ///< Output weight
    double c = 0.0; ///< Output bias
};

/// @brief Partial derivatives of the loss function J with respect to each parameter.
struct Gradients {
    double dw = 0.0; ///< dJ/dw
    double db = 0.0; ///< dJ/db
    double dv = 0.0; ///< dJ/dv
    double dc = 0.0; ///< dJ/dc
};

/// @brief Intermediate and final values produced during forward propagation.
struct ForwardOutput {
    double z = 0.0;      ///< Pre-activation: w*x + b
    double h = 0.0;      ///< Hidden activation: tanh(z)
    double y_pred = 0.0; ///< Network prediction: v*h + c
    double loss = 0.0;   ///< Quadratic loss: 0.5 * (y_pred - y)^2
};

/// @brief Record of a single training step during gradient descent.
struct TrainingStepRecord {
    size_t step = 0;                  ///< 1-based training step index (1 to N)
    double loss = 0.0;                ///< Loss at start of step
    double y_pred = 0.0;              ///< Prediction at start of step
    NetworkParameters parameters;     ///< Parameters used during this step
    Gradients gradients;              ///< Gradients calculated during this step
};

} // namespace workbench::backprop
