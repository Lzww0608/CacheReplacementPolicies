/*
@Author: Lzww
@LastEditTime: 2025-10-26 21:58:32
@Description: MGLRU PID Controller Implementation
@Language: C++17
*/

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <cstddef>

class PidController {
public:
    PidController(double p, double i, double d);
    ~PidController() = default;

    // update the metrics for the PID controller
    // refaults: number of page that are faulted and revisited in short time 
    // scanned: number of page that are scanned
    // reclaimed: number of page that are successfully collected
    void update_metrics(size_t refaults, size_t scanned, size_t reclaimed);

    size_t get_scan_intensity() const;

private:
    double Kp_, Ki_, Kd_;
    double integral_ = 0.0;
    double prev_error_ = 0.0;
    // others
};

#endif