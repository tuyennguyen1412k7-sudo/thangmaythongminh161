#include "elevator/simulator.h"

#include <algorithm>

#include "elevator/exceptions.h"
#include "elevator/text_utils.h"

void requireArgs(const ScenarioEvent& ev, std::size_t count) {
    if (ev.args.size() < count)
        throw ValidationError("Lệnh '" + ev.command + "' cần " + std::to_string(count) + " tham số");
}

void Simulator::apply(const ScenarioEvent& ev) {
    const std::string& cmd = ev.command;
    const std::vector<std::string>& a = ev.args;

    if (cmd == "passenger") {
        requireArgs(ev, 4);
        controller_.addPassenger(parseInt(a[0], "mã hành khách"), parseDouble(a[1], "khối lượng"),
                                 parseInt(a[2], "tầng đi"), parseInt(a[3], "tầng đến"));
    } else if (cmd == "hall") {
        requireArgs(ev, 2);
        controller_.callElevator(parseInt(a[0], "tầng"), parseDirection(a[1]));
    } else if (cmd == "car") {
        requireArgs(ev, 2);
        controller_.pressFloor(parseInt(a[0], "mã thang"), parseInt(a[1], "tầng"));
    } else if (cmd == "emergency") {
        requireArgs(ev, 1);
        std::string reason = (a.size() > 1) ? a[1] : "y tế";
        controller_.emergencyCall(parseInt(a[0], "tầng"), reason);
    } else if (cmd == "strategy") {
        requireArgs(ev, 1);
        controller_.setStrategy(a[0]);
    } else if (cmd == "outofservice") {
        requireArgs(ev, 1);
        controller_.setOutOfService(parseInt(a[0], "mã thang"));
    } else if (cmd == "restore") {
        requireArgs(ev, 1);
        controller_.restoreService(parseInt(a[0], "mã thang"));
    } else {
        throw ValidationError("Lệnh không hợp lệ: '" + cmd + "'");
    }
}

SimulationResult Simulator::run(std::vector<ScenarioEvent> events, long maxTicks) {
    std::sort(events.begin(), events.end(), [](const ScenarioEvent& a, const ScenarioEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.line < b.line;
    });

    SimulationResult result;
    std::size_t next = 0;

    while (result.ticks < maxTicks) {
        while (next < events.size() && events[next].tick <= controller_.now()) {
            try {
                apply(events[next]);
                ++result.commandsOk;
            } catch (const ElevatorError& ex) {
                ++result.commandsFailed;
                controller_.logError(std::string(ex.what()) + " [dòng " + std::to_string(events[next].line) + "]");
            }
            ++next;
        }

        if (next >= events.size() && controller_.idle()) return result;

        controller_.tick();
        ++result.ticks;
    }

    result.timedOut = true;
    return result;
}
