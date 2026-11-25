#pragma once
#include <string>
#include <vector>

#include "elevator/value_types.h"

struct SystemConfig {
    struct ElevatorSpec {
        int id = 0;
        int startFloor = 0;
        double capacityKg = 800.0;
    };

    int lowestFloor = 0;
    int highestFloor = 9;
    int dwellTicks = 2;
    std::string strategy = "eta";
    std::vector<ElevatorSpec> elevators;

    void validate() const;
};

struct ElevatorSnapshot {
    int id = 0;
    int floor = 0;
    Direction heading = Direction::None;
    std::vector<int> stops;
    std::vector<Passenger> onboard;
    bool outOfService = false;
};

struct Snapshot {
    long tick = 0;
    std::string strategy;
    std::vector<ElevatorSnapshot> elevators;
    std::vector<Passenger> waiting;
};

struct ScenarioEvent {
    long tick = 0;
    std::string command;
    std::vector<std::string> args;
    int line = 0;
};
