#include "elevator/elevator_state.h"

#include <memory>

#include "elevator/elevator.h"

std::unique_ptr<ElevatorState> stateFor(Direction d) {
    if (d == Direction::Up) return std::make_unique<MovingUpState>();
    if (d == Direction::Down) return std::make_unique<MovingDownState>();
    return std::make_unique<IdleState>();
}

std::unique_ptr<ElevatorState> IdleState::onTick(Elevator& e) {
    if (!e.hasWork()) return nullptr;

    if (e.shouldStopHere()) return std::make_unique<DoorOpenState>();

    Direction d = e.chooseDirection();
    if (d == Direction::None) return std::make_unique<DoorOpenState>();
    return stateFor(d);
}

std::unique_ptr<ElevatorState> MovingState::onTick(Elevator& e) {
    e.moveOneFloor(dir_);

    if (e.shouldStopHere()) return std::make_unique<DoorOpenState>();

    Direction next = e.chooseDirection();
    if (next == Direction::None) return std::make_unique<IdleState>();
    if (next != dir_) return stateFor(next);
    return nullptr;
}

void DoorOpenState::onEnter(Elevator& e) {
    e.door().open();
    e.emit(EventType::DoorOpened, "Cửa mở tại tầng " + std::to_string(e.floor()));
    e.serveCurrentFloor();
}

std::unique_ptr<ElevatorState> DoorOpenState::onTick(Elevator& e) {
    e.door().tick();

    if (e.shouldStopHere()) {
        e.serveCurrentFloor();
        e.door().open();
        return nullptr;
    }

    if (e.door().openTicks() < e.dwellTicks()) return nullptr;

    return stateFor(e.chooseDirection());
}

void DoorOpenState::onExit(Elevator& e) {
    e.door().close();
    e.emit(EventType::DoorClosed, "Cửa đóng");
}
