#include "elevator/elevator.h"

#include <algorithm>

#include "elevator/exceptions.h"

Elevator::Elevator(int id, Building& building, EventBus& bus, const long& clock, ElevatorSettings settings,
                   int startFloor)
    : id_(id),
      building_(building),
      bus_(bus),
      clock_(clock),
      settings_(settings),
      floor_(startFloor),
      state_(std::make_unique<IdleState>()) {
    building_.validate(startFloor);
    if (settings_.dwellTicks < 1) throw ValidationError("Thời gian mở cửa phải >= 1 tick");
    if (settings_.capacity.kg() <= 0.0) throw ValidationError("Tải trọng phải > 0");
}

Weight Elevator::load() const {
    Weight total;
    for (const Passenger& p : onboard_) total += p.weight;
    return total;
}

bool Elevator::isNearlyFull() const { return load() >= settings_.capacity * 0.9; }

bool Elevator::isAvailable() const { return state_->acceptsRequests(); }

bool Elevator::hasWork() const {
    if (priority_) return true;
    return !carStops_.empty() || !hall_.empty();
}

bool Elevator::isFullyIdle() const { return !hasWork() && !door_.isOpen() && onboard_.empty(); }

std::string Elevator::stateName() const { return state_->name(); }

std::set<int> Elevator::stops() const {
    std::set<int> all = carStops_;
    for (const HallAssignment& h : hall_) all.insert(h.floor);
    if (priority_) all.insert(*priority_);
    return all;
}

std::string Elevator::describe() const {
    std::string stopText;
    for (int s : stops()) {
        if (!stopText.empty()) stopText += ",";
        stopText += std::to_string(s);
    }
    if (stopText.empty()) stopText = "-";

    return "Thang " + std::to_string(id_) + " | tầng " + std::to_string(floor_) + " | " + stateName() +
           " | hướng " + toString(heading_) + " | tải " + kgText(load().kg()) + " kg/" +
           kgText(capacity().kg()) + " kg | điểm dừng: " + stopText;
}

bool Elevator::hasStopAhead(Direction d) const {
    for (int stop : stops()) {
        if (d == Direction::Up && stop > floor_) return true;
        if (d == Direction::Down && stop < floor_) return true;
    }
    return false;
}

bool Elevator::acceptsDirection(Direction d) const {
    if (d == Direction::None) return false;
    if (heading_ == Direction::None || heading_ == d) return true;
    return !hasStopAhead(heading_);
}

bool Elevator::shouldStopHere() const {
    if (priority_) return floor_ == *priority_;

    if (carStops_.find(floor_) != carStops_.end()) return true;

    for (const HallAssignment& call : hall_)
        if (call.floor == floor_ && acceptsDirection(call.direction)) return true;
    return false;
}

Direction Elevator::chooseDirection() const {
    if (priority_) {
        if (*priority_ > floor_) return Direction::Up;
        if (*priority_ < floor_) return Direction::Down;
        return Direction::None;
    }

    bool hasAbove = false;
    bool hasBelow = false;
    int nearestAbove = 0;
    int nearestBelow = 0;
    for (int stop : stops()) {
        if (stop > floor_ && !hasAbove) {
            hasAbove = true;
            nearestAbove = stop;
        }
        if (stop < floor_) {
            hasBelow = true;
            nearestBelow = stop;
        }
    }

    if (heading_ == Direction::Up && hasAbove) return Direction::Up;
    if (heading_ == Direction::Down && hasBelow) return Direction::Down;

    if (hasAbove && !hasBelow) return Direction::Up;
    if (hasBelow && !hasAbove) return Direction::Down;
    if (hasAbove && hasBelow) {
        int distanceUp = nearestAbove - floor_;
        int distanceDown = floor_ - nearestBelow;
        return (distanceUp <= distanceDown) ? Direction::Up : Direction::Down;
    }
    return Direction::None;
}

void Elevator::moveOneFloor(Direction d) {
    int next = floor_ + (d == Direction::Up ? 1 : -1);
    building_.validate(next);
    floor_ = next;
    heading_ = d;
    std::string dirText = (d == Direction::Up) ? "lên" : "xuống";
    emit(EventType::Moved, "đi " + dirText + " tầng " + std::to_string(floor_));
}

int Elevator::estimateTicksTo(int target) const {
    if (priority_) return floorDistance(*priority_, floor_) + floorDistance(target, *priority_);

    const int distance = floorDistance(target, floor_);
    const std::set<int> all = stops();
    if (all.empty()) return distance;

    const int low = (floor_ < target) ? floor_ : target;
    const int high = (floor_ < target) ? target : floor_;
    int between = std::count_if(all.begin(), all.end(), [low, high](int s) { return s > low && s < high; });

    int lowestStop = *all.begin();
    int highestStop = lowestStop;
    for (int s : all)
        if (s > highestStop) highestStop = s;

    int detour = 0;
    if (heading_ == Direction::Up && target < floor_ && highestStop > floor_)
        detour = 2 * (highestStop - floor_);
    else if (heading_ == Direction::Down && target > floor_ && lowestStop < floor_)
        detour = 2 * (floor_ - lowestStop);

    return distance + detour + between * (settings_.dwellTicks + 1);
}

void Elevator::assign(const Request& r) {
    building_.validate(r.floor());
    if (!state_->acceptsRequests()) throw ElevatorUnavailableError(id_);

    if (r.priority() >= kEmergencyPriority) {
        priority_ = r.floor();
    } else if (r.direction() != Direction::None) {
        HallAssignment call{r.floor(), r.direction()};
        if (std::find(hall_.begin(), hall_.end(), call) == hall_.end()) hall_.push_back(call);
    } else {
        carStops_.insert(r.floor());
    }
    emit(EventType::RequestAssigned, "nhận yêu cầu: " + r.describe());
}

void Elevator::board(Passenger p) {
    const Weight newLoad = load() + p.weight;
    if (newLoad > settings_.capacity) throw OverloadError(newLoad.kg(), settings_.capacity.kg());
    building_.validate(p.destination);

    p.boardTick = clock_;
    onboard_.push_back(p);
    carStops_.insert(p.destination);
    heading_ = p.direction();
    emit(EventType::PassengerBoarded,
         "hành khách #" + std::to_string(p.id) + " (" + kgText(p.weight.kg()) + " kg) lên thang, đi tầng " +
             std::to_string(p.destination),
         p.boardTick - p.arrivalTick);
}

void Elevator::unloadPassengers() {
    std::vector<Passenger> stayOn;
    for (const Passenger& p : onboard_) {
        if (p.destination == floor_) {
            emit(EventType::PassengerExited,
                 "hành khách #" + std::to_string(p.id) + " xuống tại tầng " + std::to_string(floor_),
                 clock_ - p.boardTick);
        } else {
            stayOn.push_back(p);
        }
    }
    onboard_ = stayOn;
}

void Elevator::loadPassengers() {
    Floor& currentFloor = building_.floor(floor_);

    std::vector<Passenger> waitingNow;
    for (const Passenger& p : currentFloor.waiting()) waitingNow.push_back(p);

    std::vector<Direction> rejectedDirections;
    for (const Passenger& p : waitingNow) {
        if (!acceptsDirection(p.direction())) continue;

        try {
            board(p);
            currentFloor.removeWaiting(p.id);
        } catch (const OverloadError& ex) {
            emit(EventType::OverloadRejected, "hành khách #" + std::to_string(p.id) + " ở lại tầng " +
                                                  std::to_string(floor_) + " (" + ex.what() + ")");
            if (std::find(rejectedDirections.begin(), rejectedDirections.end(), p.direction()) ==
                rejectedDirections.end())
                rejectedDirections.push_back(p.direction());
        }
    }

    for (Direction d : rejectedDirections)
        if (currentFloor.waitingCount(d) > 0) recalls_.push_back(HallAssignment{floor_, d});
}

void Elevator::serveCurrentFloor() {
    carStops_.erase(floor_);
    if (priority_ && *priority_ == floor_) priority_ = std::nullopt;

    unloadPassengers();

    hall_.erase(std::remove_if(hall_.begin(), hall_.end(),
                               [this](const HallAssignment& call) {
                                   return call.floor == floor_ && acceptsDirection(call.direction);
                               }),
                hall_.end());

    loadPassengers();
}

std::vector<HallAssignment> Elevator::setOutOfService() {
    std::vector<HallAssignment> released = hall_;
    std::vector<Passenger> stranded;
    for (const Passenger& p : onboard_) {
        if (p.destination == floor_) continue;
        Passenger waiting = p;
        waiting.origin = floor_;
        waiting.arrivalTick = clock_;
        waiting.boardTick = -1;
        stranded.push_back(waiting);

        HallAssignment call{floor_, waiting.direction()};
        if (std::find(released.begin(), released.end(), call) == released.end()) released.push_back(call);
    }

    for (const Passenger& w : stranded) {
        building_.floor(floor_).addWaiting(w);
        emit(EventType::Info,
             "hành khách #" + std::to_string(w.id) + " được sơ tán tại tầng " + std::to_string(floor_));
    }
    onboard_.clear();
    carStops_.clear();
    hall_.clear();
    recalls_.clear();
    priority_ = std::nullopt;
    door_.close();
    changeState(std::make_unique<OutOfServiceState>());
    return released;
}

void Elevator::restoreService() {
    if (isAvailable()) return;
    changeState(std::make_unique<IdleState>());
}

std::vector<HallAssignment> Elevator::takeRecalls() {
    std::vector<HallAssignment> result = recalls_;
    recalls_.clear();
    return result;
}

void Elevator::tick() {
    std::unique_ptr<ElevatorState> next = state_->onTick(*this);
    if (next) changeState(std::move(next));
}

void Elevator::changeState(std::unique_ptr<ElevatorState> next) {
    const std::string from = state_->name();
    state_->onExit(*this);
    state_ = std::move(next);
    emit(EventType::StateChanged, from + " -> " + state_->name());
    state_->onEnter(*this);
}

void Elevator::emit(EventType type, const std::string& message, long value) const {
    bus_.publish(Event{clock_, type, id_, floor_, message, value});
}

ElevatorSnapshot Elevator::snapshot() const {
    ElevatorSnapshot s;
    s.id = id_;
    s.floor = floor_;
    s.heading = heading_;
    for (int stop : stops()) s.stops.push_back(stop);
    s.onboard = onboard_;
    s.outOfService = !isAvailable();
    return s;
}

void Elevator::restore(const ElevatorSnapshot& s) {
    floor_ = s.floor;
    heading_ = s.heading;
    onboard_ = s.onboard;
    hall_.clear();
    recalls_.clear();
    priority_ = std::nullopt;
    door_.close();
    carStops_.clear();
    for (int stop : s.stops) carStops_.insert(stop);
    if (s.outOfService)
        state_ = std::make_unique<OutOfServiceState>();
    else
        state_ = std::make_unique<IdleState>();
}
