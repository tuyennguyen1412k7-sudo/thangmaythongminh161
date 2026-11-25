#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "elevator/elevator.h"
#include "elevator/request.h"

class DispatchStrategy {
public:
    virtual ~DispatchStrategy() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;

    virtual const Elevator* select(const Request& request,
                                   const std::vector<const Elevator*>& candidates) const = 0;
};

class NearestCarStrategy : public DispatchStrategy {
public:
    std::string name() const override { return "nearest"; }
    std::string description() const override { return "Thang gần tầng gọi nhất"; }
    const Elevator* select(const Request& r, const std::vector<const Elevator*>& c) const override;
};

class EtaStrategy : public DispatchStrategy {
public:
    std::string name() const override { return "eta"; }
    std::string description() const override { return "Thang có thời gian đến ước tính ngắn nhất"; }
    const Elevator* select(const Request& r, const std::vector<const Elevator*>& c) const override;
};

class LeastLoadStrategy : public DispatchStrategy {
public:
    std::string name() const override { return "least-load"; }
    std::string description() const override { return "Thang ít việc nhất (cân bằng tải)"; }
    const Elevator* select(const Request& r, const std::vector<const Elevator*>& c) const override;
};

class StrategyFactory {
public:
    using Creator = std::function<std::unique_ptr<DispatchStrategy>()>;

    StrategyFactory();

    void registerStrategy(const std::string& name, Creator creator);

    std::unique_ptr<DispatchStrategy> create(const std::string& name) const;
    std::vector<std::string> names() const;

private:
    std::map<std::string, Creator> creators_;
};
