#include "elevator/building.h"

#include <algorithm>

#include "elevator/exceptions.h"

bool Floor::removeWaiting(int passengerId) {
    auto it = std::find_if(waiting_.begin(), waiting_.end(),
                           [passengerId](const Passenger& p) { return p.id == passengerId; });
    if (it == waiting_.end()) return false;
    waiting_.erase(it);
    return true;
}

int Floor::waitingCount(Direction d) const {
    return std::count_if(waiting_.begin(), waiting_.end(), [d](const Passenger& p) { return p.direction() == d; });
}

std::string makeLabel(int floor) {
    if (floor < 0) return "B" + std::to_string(-floor);
    if (floor == 0) return "G";
    return std::to_string(floor);
}

Building::Building(int lowest, int highest) : lowest_(lowest), highest_(highest) {
    if (lowest >= highest) throw ValidationError("Toà nhà phải có ít nhất 2 tầng (lowest < highest)");
    for (int f = lowest; f <= highest; ++f) floors_.push_back(Floor(f, makeLabel(f)));
}

void Building::validate(int floor) const {
    if (!isValid(floor)) throw InvalidFloorError(floor, lowest_, highest_);
}

Floor& Building::floor(int number) {
    validate(number);
    return floors_[number - lowest_];
}

const Floor& Building::floor(int number) const {
    validate(number);
    return floors_[number - lowest_];
}

int Building::waitingPassengers() const {
    int total = 0;
    for (const Floor& f : floors_) total += f.waiting().size();
    return total;
}
