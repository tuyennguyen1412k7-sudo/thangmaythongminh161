#include "elevator/dispatch_strategy.h"

#include "elevator/exceptions.h"

const Elevator* pickLowestScore(const std::vector<const Elevator*>& candidates,
                                const std::function<int(const Elevator&)>& score) {
    const Elevator* best = nullptr;
    int bestScore = 0;
    for (const Elevator* e : candidates) {
        int s = score(*e);
        if (best == nullptr || s < bestScore || (s == bestScore && e->id() < best->id())) {
            best = e;
            bestScore = s;
        }
    }
    return best;
}

const Elevator* NearestCarStrategy::select(const Request& r, const std::vector<const Elevator*>& c) const {
    return pickLowestScore(c, [&r](const Elevator& e) { return floorDistance(e.floor(), r.floor()); });
}

const Elevator* EtaStrategy::select(const Request& r, const std::vector<const Elevator*>& c) const {
    return pickLowestScore(c, [&r](const Elevator& e) { return e.estimateTicksTo(r.floor()); });
}

const Elevator* LeastLoadStrategy::select(const Request& r, const std::vector<const Elevator*>& c) const {
    return pickLowestScore(c, [&r](const Elevator& e) {
        int work = e.stops().size() + e.passengerCount();
        return work * 1000 + floorDistance(e.floor(), r.floor());
    });
}

StrategyFactory::StrategyFactory() {
    registerStrategy("nearest", [] { return std::make_unique<NearestCarStrategy>(); });
    registerStrategy("eta", [] { return std::make_unique<EtaStrategy>(); });
    registerStrategy("least-load", [] { return std::make_unique<LeastLoadStrategy>(); });
}

void StrategyFactory::registerStrategy(const std::string& name, Creator creator) {
    if (name.empty() || !creator) throw ValidationError("Đăng ký strategy không hợp lệ");
    creators_[name] = creator;
}

std::unique_ptr<DispatchStrategy> StrategyFactory::create(const std::string& name) const {
    if (creators_.find(name) == creators_.end()) {
        std::string known;
        for (const std::string& n : names()) known += (known.empty() ? "" : ", ") + n;
        throw ValidationError("Không có chiến lược điều phối '" + name + "' (có: " + known + ")");
    }
    return creators_.at(name)();
}

std::vector<std::string> StrategyFactory::names() const {
    std::vector<std::string> result;
    for (const auto& [name, creator] : creators_) result.push_back(name);
    return result;
}
