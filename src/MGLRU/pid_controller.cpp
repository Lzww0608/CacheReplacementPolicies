/*
@Author: Lzww
@LastEditTime: 2025-10-26 21:58:32  
@Description: PID Controller for MGLRU scan intensity
@Language: C++17
*/

#include "pid_controller.h"
#include <algorithm>

PidController::PidController(double p, double i, double d) 
    : Kp_(p), Ki_(i), Kd_(d) {
}

void PidController::update_metrics(size_t refaults, size_t scanned, size_t reclaimed) {
    (void)scanned;  // Reserved for future use
    
    // Calculate error: we want to minimize refaults while maintaining efficiency
    // Error is the ratio of refaults to reclaimed pages
    double error = 0.0;
    if (reclaimed > 0) {
        error = static_cast<double>(refaults) / static_cast<double>(reclaimed);
    }
    
    // PID control
    integral_ += error;
    // double derivative = error - prev_error_;  // Reserved for future use
    prev_error_ = error;
    
    // Clamp integral to prevent windup
    integral_ = std::clamp(integral_, -100.0, 100.0);
}

size_t PidController::get_scan_intensity() const {
    // Calculate PID output
    double output = Kp_ * prev_error_ + Ki_ * integral_ + Kd_ * (prev_error_ - integral_);
    
    // Convert to scan intensity (number of pages to scan)
    // Base intensity + adjustment based on PID output
    size_t base_intensity = 32;  // scan 32 pages by default
    double adjustment = output * 10.0;
    
    size_t intensity = base_intensity + static_cast<size_t>(std::max(0.0, adjustment));
    
    // Clamp to reasonable range
    return std::clamp(intensity, size_t(1), size_t(1024));
}

