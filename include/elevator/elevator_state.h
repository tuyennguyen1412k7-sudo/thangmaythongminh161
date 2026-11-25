#pragma once
#include <memory>
#include <string>

#include "elevator/value_types.h"

class Elevator;

class ElevatorState {
public:
    virtual ~ElevatorState() = default;

    virtual std::string name() const = 0;
    virtual void onEnter(Elevator&) {}
    virtual void onExit(Elevator&) {}
    virtual std::unique_ptr<ElevatorState> onTick(Elevator& e) = 0;

    virtual bool acceptsRequests() const { return true; }
};

class IdleState : public ElevatorState {
public:
    std::string name() const override { return "Chờ"; }
    std::unique_ptr<ElevatorState> onTick(Elevator& e) override;
};

class MovingState : public ElevatorState {
public:
    explicit MovingState(Direction d) : dir_(d) {}
    Direction direction() const { return dir_; }
    std::unique_ptr<ElevatorState> onTick(Elevator& e) override;

private:
    Direction dir_;
};

class MovingUpState : public MovingState {
public:
    MovingUpState() : MovingState(Direction::Up) {}
    std::string name() const override { return "Đang lên"; }
};

class MovingDownState : public MovingState {
public:
    MovingDownState() : MovingState(Direction::Down) {}
    std::string name() const override { return "Đang xuống"; }
};

class DoorOpenState : public ElevatorState {
public:
    std::string name() const override { return "Mở cửa"; }
    void onEnter(Elevator& e) override;
    std::unique_ptr<ElevatorState> onTick(Elevator& e) override;
    void onExit(Elevator& e) override;
};

class OutOfServiceState : public ElevatorState {
public:
    std::string name() const override { return "Ngừng hoạt động"; }
    std::unique_ptr<ElevatorState> onTick(Elevator&) override { return nullptr; }
    bool acceptsRequests() const override { return false; }
};
