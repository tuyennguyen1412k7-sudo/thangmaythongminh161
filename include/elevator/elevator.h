#pragma once
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "elevator/building.h"
#include "elevator/config.h"
#include "elevator/door.h"
#include "elevator/elevator_state.h"
#include "elevator/event.h"
#include "elevator/request.h"
#include "elevator/value_types.h"

struct ElevatorSettings {
    Weight capacity{800.0};
    int dwellTicks = 2;
};

struct HallAssignment {
    int floor = 0;
    Direction direction = Direction::None;

    bool operator==(const HallAssignment& other) const {
        return floor == other.floor && direction == other.direction;
    }
};

class Elevator {
public:
    Elevator(int id, Building& building, EventBus& bus, const long& clock, ElevatorSettings settings,
             int startFloor);

    Elevator(const Elevator&) = delete;
    Elevator& operator=(const Elevator&) = delete;

    int id() const { return id_; }
    int floor() const { return floor_; }
    Direction heading() const { return heading_; }
    const Weight& capacity() const { return settings_.capacity; }
    int dwellTicks() const { return settings_.dwellTicks; }
    long now() const { return clock_; }

    Weight load() const;
    int passengerCount() const { return onboard_.size(); }
    bool isNearlyFull() const;
    bool isAvailable() const;
    bool hasWork() const;
    bool isFullyIdle() const;
    std::string stateName() const;
    std::set<int> stops() const;
    int estimateTicksTo(int floor) const;
    std::string describe() const;

    void assign(const Request& request);
    void board(Passenger p);
    std::vector<HallAssignment> setOutOfService();
    void restoreService();
    std::vector<HallAssignment> takeRecalls();
    void tick();

    ElevatorSnapshot snapshot() const;
    void restore(const ElevatorSnapshot& s);

    Door& door() { return door_; }
    const Door& door() const { return door_; }
    bool shouldStopHere() const;
    Direction chooseDirection() const;
    void moveOneFloor(Direction d);
    void serveCurrentFloor();
    void emit(EventType type, const std::string& message, long value = 0) const;

private:
    bool acceptsDirection(Direction d) const;
    bool hasStopAhead(Direction d) const;
    void changeState(std::unique_ptr<ElevatorState> next);
    void unloadPassengers();
    void loadPassengers();

    int id_;
    Building& building_;
    EventBus& bus_;
    const long& clock_;
    ElevatorSettings settings_;
    int floor_;
    Direction heading_ = Direction::None;
    Door door_;
    std::vector<Passenger> onboard_;
    std::set<int> carStops_;
    std::vector<HallAssignment> hall_;
    std::optional<int> priority_;
    std::vector<HallAssignment> recalls_;
    std::unique_ptr<ElevatorState> state_;
};
