#pragma once
#include <memory>
#include <string>

#include "elevator/value_types.h"

const int kNormalPriority = 1;
const int kEmergencyPriority = 10;

class Request {
public:
    Request(int id, int floor, long createdTick) : id_(id), floor_(floor), createdTick_(createdTick) {}

    virtual ~Request() = default;

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    int id() const { return id_; }
    int floor() const { return floor_; }
    long createdTick() const { return createdTick_; }

    virtual std::string kind() const = 0;
    virtual int priority() const = 0;
    virtual Direction direction() const { return Direction::None; }
    virtual std::string describe() const = 0;

private:
    int id_;
    int floor_;
    long createdTick_;
};

class HallCall : public Request {
public:
    HallCall(int id, int floor, Direction dir, long tick);

    std::string kind() const override { return "hall"; }
    int priority() const override { return kNormalPriority; }
    Direction direction() const override { return dir_; }
    std::string describe() const override;

private:
    Direction dir_;
};

class CarCall : public Request {
public:
    CarCall(int id, int elevatorId, int floor, long tick)
        : Request(id, floor, tick), elevatorId_(elevatorId) {}

    int elevatorId() const { return elevatorId_; }
    std::string kind() const override { return "car"; }
    int priority() const override { return kNormalPriority; }
    std::string describe() const override;

private:
    int elevatorId_;
};

class EmergencyCall : public Request {
public:
    EmergencyCall(int id, int floor, long tick, const std::string& reason)
        : Request(id, floor, tick), reason_(reason) {}

    std::string kind() const override { return "emergency"; }
    int priority() const override { return kEmergencyPriority; }
    std::string describe() const override;

private:
    std::string reason_;
};

class RequestFactory {
public:
    std::unique_ptr<Request> makeHall(int floor, Direction dir, long tick);
    std::unique_ptr<Request> makeCar(int elevatorId, int floor, long tick);
    std::unique_ptr<Request> makeEmergency(int floor, long tick, const std::string& reason);

    int issued() const { return nextId_ - 1; }

private:
    int nextId_ = 1;
};
