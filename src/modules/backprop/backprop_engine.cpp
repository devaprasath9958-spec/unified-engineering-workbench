#include "modules/backprop/backprop_engine.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

namespace workbench::backprop {

ForwardOutput forward_pass(double x, double y, const NetworkParameters& params) {
    ForwardOutput out;
    out.z = params.w * x + params.b;
    out.h = std::tanh(out.z);
    out.y_pred = params.v * out.h + params.c;
    const double error = out.y_pred - y;
    out.loss = 0.5 * error * error;
    return out;
}

Gradients compute_analytical_gradients(double x, double y, const NetworkParameters& params) {
    // Forward values using original parameters
    const double z = params.w * x + params.b;
    const double h = std::tanh(z);
    const double y_pred = params.v * h + params.c;

    // Output layer error: dJ/dy_pred = y_pred - y
    const double delta_out = y_pred - y;

    // Gradients for output layer parameters
    const double dc = delta_out * 1.0;
    const double dv = delta_out * h;

    // Backpropagate error to hidden pre-activation: dJ/dz = delta_out * v * (1 - h^2)
    const double dtanh = 1.0 - h * h;
    const double delta_hidden = delta_out * params.v * dtanh;

    // Gradients for hidden layer parameters
    const double dw = delta_hidden * x;
    const double db = delta_hidden * 1.0;

    return Gradients{dw, db, dv, dc};
}

Gradients compute_numerical_gradients(double x, double y, const NetworkParameters& params, double epsilon) {
    const double inv_2eps = 1.0 / (2.0 * epsilon);

    // 1. Partial w
    NetworkParameters pw_p = params; pw_p.w += epsilon;
    NetworkParameters pw_m = params; pw_m.w -= epsilon;
    const double dw = (forward_pass(x, y, pw_p).loss - forward_pass(x, y, pw_m).loss) * inv_2eps;

    // 2. Partial b
    NetworkParameters pb_p = params; pb_p.b += epsilon;
    NetworkParameters pb_m = params; pb_m.b -= epsilon;
    const double db = (forward_pass(x, y, pb_p).loss - forward_pass(x, y, pb_m).loss) * inv_2eps;

    // 3. Partial v
    NetworkParameters pv_p = params; pv_p.v += epsilon;
    NetworkParameters pv_m = params; pv_m.v -= epsilon;
    const double dv = (forward_pass(x, y, pv_p).loss - forward_pass(x, y, pv_m).loss) * inv_2eps;

    // 4. Partial c
    NetworkParameters pc_p = params; pc_p.c += epsilon;
    NetworkParameters pc_m = params; pc_m.c -= epsilon;
    const double dc = (forward_pass(x, y, pc_p).loss - forward_pass(x, y, pc_m).loss) * inv_2eps;

    return Gradients{dw, db, dv, dc};
}

NetworkParameters update_parameters_simultaneous(
    const NetworkParameters& params,
    const Gradients& grads,
    double learning_rate
) {
    NetworkParameters updated;
    updated.w = params.w - learning_rate * grads.dw;
    updated.b = params.b - learning_rate * grads.db;
    updated.v = params.v - learning_rate * grads.dv;
    updated.c = params.c - learning_rate * grads.dc;
    return updated;
}

GradientVerificationResult verify_gradients(
    double x,
    double y,
    const NetworkParameters& params,
    double epsilon,
    double tolerance
) {
    GradientVerificationResult res;
    res.analytical = compute_analytical_gradients(x, y, params);
    res.numerical = compute_numerical_gradients(x, y, params, epsilon);

    res.absolute_errors.dw = std::abs(res.analytical.dw - res.numerical.dw);
    res.absolute_errors.db = std::abs(res.analytical.db - res.numerical.db);
    res.absolute_errors.dv = std::abs(res.analytical.dv - res.numerical.dv);
    res.absolute_errors.dc = std::abs(res.analytical.dc - res.numerical.dc);

    res.max_absolute_error = std::max({
        res.absolute_errors.dw,
        res.absolute_errors.db,
        res.absolute_errors.dv,
        res.absolute_errors.dc
    });

    res.passed = (res.max_absolute_error <= tolerance);
    return res;
}

std::vector<TrainingStepRecord> train_network(
    double x,
    double y,
    NetworkParameters initial_params,
    double learning_rate,
    size_t steps
) {
    std::vector<TrainingStepRecord> history;
    history.reserve(steps);

    NetworkParameters current_params = initial_params;

    for (size_t s = 1; s <= steps; ++s) {
        const ForwardOutput fwd = forward_pass(x, y, current_params);
        const Gradients grads = compute_analytical_gradients(x, y, current_params);

        TrainingStepRecord rec;
        rec.step = s;
        rec.loss = fwd.loss;
        rec.y_pred = fwd.y_pred;
        rec.parameters = current_params;
        rec.gradients = grads;
        history.push_back(rec);

        // Simultaneous parameter update using original parameters and gradients of this step
        current_params = update_parameters_simultaneous(current_params, grads, learning_rate);
    }

    return history;
}

core::Status export_loss_history_csv(
    const std::string& filepath,
    const std::vector<TrainingStepRecord>& history
) {
    if (filepath.empty()) {
        return core::Status::failure(
            core::StatusCode::InvalidInput,
            "Cannot export loss history: filepath is empty."
        );
    }

    // Ensure parent directory exists
    try {
        const std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (const std::exception& ex) {
        return core::Status::failure(
            core::StatusCode::ComputationError,
            std::string("Failed to create parent directories for loss history CSV: ") + ex.what()
        );
    }

    std::ofstream out(filepath);
    if (!out.is_open()) {
        return core::Status::failure(
            core::StatusCode::FileNotFound,
            "Failed to open or create loss history CSV file: " + filepath
        );
    }

    // Clearly defined header columns
    out << "step,loss,y_pred,w,b,v,c,dw,db,dv,dc\n";

    out << std::fixed << std::setprecision(12);
    for (const auto& rec : history) {
        out << rec.step << ","
            << rec.loss << ","
            << rec.y_pred << ","
            << rec.parameters.w << ","
            << rec.parameters.b << ","
            << rec.parameters.v << ","
            << rec.parameters.c << ","
            << rec.gradients.dw << ","
            << rec.gradients.db << ","
            << rec.gradients.dv << ","
            << rec.gradients.dc << "\n";
    }

    if (out.fail()) {
        return core::Status::failure(
            core::StatusCode::ComputationError,
            "Stream write failure occurred while writing loss history CSV: " + filepath
        );
    }

    return core::Status::ok();
}

} // namespace workbench::backprop
