#pragma once
#include <deque>
#include <string>
#include <vector>

#include "elevator/value_types.h"

class Floor {
public:
    Floor(int number, const std::string& label) : number_(number), label_(label) {}

    int number() const { return number_; }
    const std::string& label() const { return label_; }

    const std::deque<Passenger>& waiting() const { return waiting_; }
    void addWaiting(const Passenger& p) { waiting_.push_back(p); }
    bool removeWaiting(int passengerId);
    void clearWaiting() { waiting_.clear(); }
    int waitingCount(Direction d) const;

private:
    int number_;
    std::string label_;
    std::deque<Passenger> waiting_;
};

class Building {
public:
    Building(int lowest, int highest);

    int lowest() const { return lowest_; }
    int highest() const { return highest_; }
    int floorCount() const { return floors_.size(); }

    bool isValid(int floor) const { return floor >= lowest_ && floor <= highest_; }
    void validate(int floor) const;

    Floor& floor(int number);
    const Floor& floor(int number) const;
    const std::vector<Floor>& floors() const { return floors_; }

    int waitingPassengers() const;

private:
    int lowest_;
    int highest_;
    std::vector<Floor> floors_;
};
