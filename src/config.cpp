#include "elevator/config.h"

#include <set>

#include "elevator/exceptions.h"

void SystemConfig::validate() const {
    if (lowestFloor >= highestFloor)
        throw ValidationError("Cấu hình: tầng thấp nhất phải nhỏ hơn tầng cao nhất");
    if (dwellTicks < 1) throw ValidationError("Cấu hình: thời gian mở cửa (dwell) phải >= 1 tick");
    if (elevators.empty()) throw ValidationError("Cấu hình: cần ít nhất 1 thang máy");

    std::set<int> usedIds;
    for (const ElevatorSpec& e : elevators) {
        if (usedIds.find(e.id) != usedIds.end())
            throw ValidationError("Cấu hình: trùng mã thang máy " + std::to_string(e.id));
        usedIds.insert(e.id);
        if (e.startFloor < lowestFloor || e.startFloor > highestFloor)
            throw InvalidFloorError(e.startFloor, lowestFloor, highestFloor);
        if (e.capacityKg <= 0.0)
            throw ValidationError("Cấu hình: tải trọng thang " + std::to_string(e.id) + " phải > 0");
    }
}
