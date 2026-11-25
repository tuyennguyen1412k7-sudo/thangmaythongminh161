#include "elevator/controller.h"

#include <algorithm>
#include <utility>

#include "elevator/exceptions.h"

Building makeBuilding(const SystemConfig& config) {
    config.validate();
    return Building(config.lowestFloor, config.highestFloor);
}

ElevatorController::ElevatorController(const SystemConfig& config)
    : building_(makeBuilding(config)), strategy_(strategyFactory_.create(config.strategy)) {
    for (const SystemConfig::ElevatorSpec& spec : config.elevators) {
        ElevatorSettings settings{Weight{spec.capacityKg}, config.dwellTicks};
        elevators_.push_back(
            std::make_unique<Elevator>(spec.id, building_, bus_, tick_, settings, spec.startFloor));
    }
}

void ElevatorController::setStrategy(const std::string& name) {
    auto next = strategyFactory_.create(name);
    strategy_ = std::move(next);
    logInfo("Đổi chiến lược điều phối: " + strategy_->name() + " — " + strategy_->description());
}

void ElevatorController::callElevator(int floor, Direction dir) {
    building_.validate(floor);
    if (dir == Direction::None) throw ValidationError("Gọi thang cần chỉ rõ hướng lên/xuống");

    bool goingUpFromTop = (dir == Direction::Up && floor == building_.highest());
    bool goingDownFromBottom = (dir == Direction::Down && floor == building_.lowest());
    if (goingUpFromTop || goingDownFromBottom)
        throw ValidationError("Không thể gọi thang đi " + toString(dir) + " ở tầng " + std::to_string(floor));

    dispatch(requestFactory_.makeHall(floor, dir, tick_));
}

void ElevatorController::pressFloor(int elevatorId, int floor) {
    Elevator& e = elevator(elevatorId);
    building_.validate(floor);

    auto request = requestFactory_.makeCar(elevatorId, floor, tick_);
    e.assign(*request);
    requestLog_.push_back(std::move(request));
}

void ElevatorController::emergencyCall(int floor, const std::string& reason) {
    building_.validate(floor);
    dispatch(requestFactory_.makeEmergency(floor, tick_, reason));
}

void ElevatorController::addPassenger(int id, double weightKg, int from, int to) {
    building_.validate(from);
    building_.validate(to);
    if (from == to) throw ValidationError("Hành khách #" + std::to_string(id) + ": tầng đi và đến trùng nhau");

    Passenger p;
    p.id = id;
    p.weight = Weight{weightKg};
    p.origin = from;
    p.destination = to;
    p.arrivalTick = tick_;

    building_.floor(from).addWaiting(p);
    try {
        callElevator(from, p.direction());
    } catch (...) {
        building_.floor(from).removeWaiting(id);
        throw;
    }

    lastPassengerId_ = std::max(lastPassengerId_, id);
    logInfo("Hành khách #" + std::to_string(id) + " (" + kgText(weightKg) + " kg) chờ ở tầng " +
            std::to_string(from) + ", muốn tới tầng " + std::to_string(to));
}

int ElevatorController::addPassenger(double weightKg, int from, int to) {
    const int id = lastPassengerId_ + 1;
    addPassenger(id, weightKg, from, to);
    return id;
}

void ElevatorController::setOutOfService(int elevatorId) {
    Elevator& e = elevator(elevatorId);
    if (!e.isAvailable()) throw ElevatorUnavailableError(elevatorId);

    std::vector<HallAssignment> released = e.setOutOfService();
    logInfo("Thang " + std::to_string(elevatorId) + " ngừng hoạt động, phân lại " +
            std::to_string(released.size()) + " yêu cầu sảnh");
    for (const HallAssignment& call : released) dispatch(requestFactory_.makeHall(call.floor, call.direction, tick_));
}

void ElevatorController::restoreService(int elevatorId) {
    elevator(elevatorId).restoreService();
    logInfo("Thang " + std::to_string(elevatorId) + " hoạt động trở lại");
}

void ElevatorController::tick() {
    ++tick_;
    dispatchPending();

    for (auto& e : elevators_) {
        e->tick();

        for (const HallAssignment& call : e->takeRecalls())
            dispatch(requestFactory_.makeHall(call.floor, call.direction, tick_), e->id());
    }
}

bool ElevatorController::idle() const {
    if (!pending_.empty() || building_.waitingPassengers() > 0) return false;
    for (const auto& e : elevators_)
        if (!e->isFullyIdle()) return false;
    return true;
}

Request& ElevatorController::remember(std::unique_ptr<Request> request) {
    requestLog_.push_back(std::move(request));
    return *requestLog_.back();
}

void ElevatorController::dispatch(std::unique_ptr<Request> request, int excludeElevator) {
    const Request& req = remember(std::move(request));
    bus_.publish(Event{tick_, EventType::RequestReceived, -1, req.floor(), req.describe(), 0});

    if (!tryAssign(req, excludeElevator)) {
        pending_.push_back(PendingRequest{&req, excludeElevator});
        bus_.publish(Event{tick_, EventType::RequestQueued, -1, req.floor(),
                           "chưa có thang phù hợp, xếp hàng chờ: " + req.describe(), 0});
    }
}

bool ElevatorController::tryAssign(const Request& r, int excludeElevator) {
    std::vector<const Elevator*> candidates;
    for (const auto& e : elevators_) {
        if (!e->isAvailable() || e->isNearlyFull()) continue;
        if (e->id() == excludeElevator && e->floor() == r.floor()) continue;
        candidates.push_back(e.get());
    }

    const Elevator* chosen = strategy_->select(r, candidates);
    if (chosen == nullptr) return false;

    elevator(chosen->id()).assign(r);
    return true;
}

void ElevatorController::dispatchPending() {
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (tryAssign(*it->request, it->excludeElevator))
            it = pending_.erase(it);
        else
            ++it;
    }
}

Elevator& ElevatorController::elevator(int id) {
    for (auto& e : elevators_)
        if (e->id() == id) return *e;
    throw ValidationError("Không tồn tại thang máy " + std::to_string(id));
}

const Elevator& ElevatorController::elevator(int id) const {
    for (const auto& e : elevators_)
        if (e->id() == id) return *e;
    throw ValidationError("Không tồn tại thang máy " + std::to_string(id));
}

std::vector<const Request*> ElevatorController::findRequests(const std::string& kind) const {
    std::vector<const Request*> result;
    for (const auto& r : requestLog_)
        if (r->kind() == kind) result.push_back(r.get());
    return result;
}

std::vector<const Elevator*> ElevatorController::elevatorsByLoad() const {
    std::vector<const Elevator*> result;
    for (const auto& e : elevators_) result.push_back(e.get());

    std::sort(result.begin(), result.end(), [](const Elevator* a, const Elevator* b) {
        if (a->load() == b->load()) return a->id() < b->id();
        return a->load() > b->load();
    });
    return result;
}

std::vector<Passenger> ElevatorController::waitingList() const {
    std::vector<Passenger> result;
    for (const Floor& f : building_.floors())
        for (const Passenger& p : f.waiting()) result.push_back(p);

    std::sort(result.begin(), result.end(), [](const Passenger& a, const Passenger& b) {
        if (a.arrivalTick == b.arrivalTick) return a.id < b.id;
        return a.arrivalTick < b.arrivalTick;
    });
    return result;
}

Snapshot ElevatorController::snapshot() const {
    Snapshot s;
    s.tick = tick_;
    s.strategy = strategy_->name();
    for (const auto& e : elevators_) s.elevators.push_back(e->snapshot());
    for (const Floor& f : building_.floors())
        for (const Passenger& p : f.waiting()) s.waiting.push_back(p);
    return s;
}

void ElevatorController::checkSnapshot(const Snapshot& s) const {
    if (s.tick < 0) throw ValidationError("Snapshot: tick âm");
    if (s.elevators.size() != elevators_.size())
        throw ValidationError("Snapshot: số thang (" + std::to_string(s.elevators.size()) +
                              ") khác hệ thống hiện tại (" + std::to_string(elevators_.size()) + ")");

    for (const ElevatorSnapshot& es : s.elevators) {
        const Elevator& e = elevator(es.id);
        building_.validate(es.floor);
        for (int stop : es.stops) building_.validate(stop);

        Weight total;
        for (const Passenger& p : es.onboard) {
            building_.validate(p.destination);
            total += p.weight;
        }
        if (total > e.capacity()) throw OverloadError(total.kg(), e.capacity().kg());
    }

    for (const Passenger& p : s.waiting) {
        building_.validate(p.origin);
        building_.validate(p.destination);
        if (p.origin == p.destination) throw ValidationError("Snapshot: hành khách có tầng đi trùng tầng đến");
    }
}

void ElevatorController::restore(const Snapshot& s) {
    auto newStrategy = strategyFactory_.create(s.strategy);
    checkSnapshot(s);

    strategy_ = std::move(newStrategy);
    tick_ = s.tick;
    pending_.clear();
    for (const ElevatorSnapshot& es : s.elevators) {
        elevator(es.id).restore(es);
        for (const Passenger& p : es.onboard) lastPassengerId_ = std::max(lastPassengerId_, p.id);
    }
    for (const Floor& f : building_.floors()) building_.floor(f.number()).clearWaiting();
    for (const Passenger& p : s.waiting) {
        building_.floor(p.origin).addWaiting(p);
        lastPassengerId_ = std::max(lastPassengerId_, p.id);
    }

    std::vector<HallAssignment> calls;
    for (const Passenger& p : s.waiting) {
        HallAssignment call{p.origin, p.direction()};
        if (std::find(calls.begin(), calls.end(), call) == calls.end()) calls.push_back(call);
    }
    for (const HallAssignment& call : calls) dispatch(requestFactory_.makeHall(call.floor, call.direction, tick_));
}

void ElevatorController::logInfo(const std::string& message) const {
    bus_.publish(Event{tick_, EventType::Info, -1, -1, message, 0});
}

void ElevatorController::logError(const std::string& message) const {
    bus_.publish(Event{tick_, EventType::Error, -1, -1, "LỖI: " + message, 0});
}
