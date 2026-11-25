#pragma once
#include <vector>

#include "elevator/config.h"
#include "elevator/controller.h"

struct SimulationResult {
    long ticks = 0;
    int commandsOk = 0;
    int commandsFailed = 0;
    bool timedOut = false;
};

class Simulator {
public:
    explicit Simulator(ElevatorController& controller) : controller_(controller) {}

    SimulationResult run(std::vector<ScenarioEvent> events, long maxTicks = 500);

private:
    void apply(const ScenarioEvent& ev);
    ElevatorController& controller_;
};
