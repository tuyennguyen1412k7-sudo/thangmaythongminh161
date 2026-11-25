#include "elevator/request.h"

#include "elevator/exceptions.h"

HallCall::HallCall(int id, int floor, Direction dir, long tick) : Request(id, floor, tick), dir_(dir) {
    if (dir == Direction::None) throw ValidationError("Yêu cầu gọi thang phải có hướng lên hoặc xuống");
}

std::string HallCall::describe() const {
    const std::string dirText = (dir_ == Direction::Up) ? "lên" : "xuống";
    return "Gọi thang tại tầng " + std::to_string(floor()) + " (đi " + dirText + ")";
}

std::string CarCall::describe() const {
    return "Chọn tầng " + std::to_string(floor()) + " trong thang " + std::to_string(elevatorId_);
}

std::string EmergencyCall::describe() const {
    return "KHẨN CẤP tại tầng " + std::to_string(floor()) + " (" + reason_ + ")";
}

std::unique_ptr<Request> RequestFactory::makeHall(int floor, Direction dir, long tick) {
    auto request = std::make_unique<HallCall>(nextId_, floor, dir, tick);
    ++nextId_;
    return request;
}

std::unique_ptr<Request> RequestFactory::makeCar(int elevatorId, int floor, long tick) {
    auto request = std::make_unique<CarCall>(nextId_, elevatorId, floor, tick);
    ++nextId_;
    return request;
}

std::unique_ptr<Request> RequestFactory::makeEmergency(int floor, long tick, const std::string& reason) {
    auto request = std::make_unique<EmergencyCall>(nextId_, floor, tick, reason);
    ++nextId_;
    return request;
}
