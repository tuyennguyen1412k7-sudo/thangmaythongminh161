#pragma once
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "elevator/building.h"
#include "elevator/config.h"
#include "elevator/dispatch_strategy.h"
#include "elevator/elevator.h"
#include "elevator/event.h"
#include "elevator/request.h"

class ElevatorController {
public:
    explicit ElevatorController(const SystemConfig& config);
    ElevatorController(const ElevatorController&) = delete;
    ElevatorController& operator=(const ElevatorController&) = delete;

    void attach(std::shared_ptr<EventListener> listener) { bus_.attach(listener); }

    void setStrategy(const std::string& name);
    const DispatchStrategy& strategy() const { return *strategy_; }
    const StrategyFactory& strategies() const { return strategyFactory_; }

    void callElevator(int floor, Direction dir);
    void pressFloor(int elevatorId, int floor);
    void emergencyCall(int floor, const std::string& reason);
    void addPassenger(int id, double weightKg, int from, int to);
    int addPassenger(double weightKg, int from, int to);
    void setOutOfService(int elevatorId);
    void restoreService(int elevatorId);
    void tick();
    bool idle() const;

    long now() const { return tick_; }
    const Building& building() const { return building_; }
    const std::vector<std::unique_ptr<Elevator>>& elevators() const { return elevators_; }
    Elevator& elevator(int id);
    const Elevator& elevator(int id) const;
    const std::vector<std::unique_ptr<Request>>& requestLog() const { return requestLog_; }
    int pendingCount() const { return pending_.size(); }

    std::vector<const Request*> findRequests(const std::string& kind) const;
    std::vector<const Elevator*> elevatorsByLoad() const;
    std::vector<Passenger> waitingList() const;

    Snapshot snapshot() const;
    void restore(const Snapshot& s);

    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;

private:
    struct PendingRequest {
        const Request* request;
        int excludeElevator;
    };

    static const int kNoElevator = -1;

    Request& remember(std::unique_ptr<Request> request);
    void dispatch(std::unique_ptr<Request> request, int excludeElevator = kNoElevator);
    bool tryAssign(const Request& request, int excludeElevator);
    void dispatchPending();
    void checkSnapshot(const Snapshot& s) const;

    EventBus bus_;
    Building building_;
    long tick_ = 0;
    StrategyFactory strategyFactory_;
    std::unique_ptr<DispatchStrategy> strategy_;
    RequestFactory requestFactory_;
    std::vector<std::unique_ptr<Elevator>> elevators_;
    std::vector<std::unique_ptr<Request>> requestLog_;
    std::deque<PendingRequest> pending_;
    int lastPassengerId_ = 0;
};
