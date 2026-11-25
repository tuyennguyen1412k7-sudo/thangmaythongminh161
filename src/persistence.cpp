#include "elevator/persistence.h"

#include <fstream>
#include <sstream>

#include "elevator/exceptions.h"
#include "elevator/text_utils.h"

struct Record {
    int line;
    std::vector<std::string> fields;
};

std::vector<Record> readRecords(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw FileError("Không mở được file: " + path);

    std::vector<Record> records;
    std::string raw;
    int lineNumber = 0;
    while (std::getline(in, raw)) {
        ++lineNumber;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        records.push_back(Record{lineNumber, split(line, ',')});
    }
    if (in.bad()) throw FileError("Lỗi khi đọc file: " + path);
    return records;
}

void requireFields(const Record& r, std::size_t count) {
    if (r.fields.size() < count)
        throw ValidationError("thiếu trường: cần " + std::to_string(count) + ", có " + std::to_string(r.fields.size()));
}

Passenger parsePassenger(const std::vector<std::string>& f, std::size_t first) {
    Passenger p;
    p.id = parseInt(f[first], "mã hành khách");
    p.weight = Weight{parseDouble(f[first + 1], "khối lượng")};
    p.origin = parseInt(f[first + 2], "tầng đi");
    p.destination = parseInt(f[first + 3], "tầng đến");
    p.arrivalTick = parseInt(f[first + 4], "tick đến");
    return p;
}

SystemConfig loadConfig(const std::string& path) {
    SystemConfig cfg;
    bool hasBuilding = false;

    for (const Record& r : readRecords(path)) {
        try {
            const std::string key = toLower(r.fields[0]);
            if (key == "building") {
                requireFields(r, 3);
                cfg.lowestFloor = parseInt(r.fields[1], "tầng thấp nhất");
                cfg.highestFloor = parseInt(r.fields[2], "tầng cao nhất");
                hasBuilding = true;
            } else if (key == "dwell") {
                requireFields(r, 2);
                cfg.dwellTicks = parseInt(r.fields[1], "dwell");
            } else if (key == "strategy") {
                requireFields(r, 2);
                cfg.strategy = r.fields[1];
            } else if (key == "elevator") {
                requireFields(r, 4);
                SystemConfig::ElevatorSpec spec;
                spec.id = parseInt(r.fields[1], "mã thang");
                spec.startFloor = parseInt(r.fields[2], "tầng xuất phát");
                spec.capacityKg = parseDouble(r.fields[3], "tải trọng");
                cfg.elevators.push_back(spec);
            } else {
                throw ParseError(path, r.line, "từ khoá không hợp lệ: '" + r.fields[0] + "'");
            }
        } catch (const ValidationError& ex) {
            throw ParseError(path, r.line, ex.what());
        }
    }

    if (!hasBuilding) throw ValidationError("File cấu hình thiếu dòng 'building'");
    cfg.validate();
    return cfg;
}

std::vector<ScenarioEvent> loadScenario(const std::string& path) {
    std::vector<ScenarioEvent> events;

    for (const Record& r : readRecords(path)) {
        try {
            requireFields(r, 2);
            ScenarioEvent ev;
            ev.tick = parseInt(r.fields[0], "tick");
            if (ev.tick < 0) throw ParseError(path, r.line, "tick không được âm");
            ev.command = toLower(r.fields[1]);
            for (std::size_t i = 2; i < r.fields.size(); ++i) ev.args.push_back(r.fields[i]);
            ev.line = r.line;
            events.push_back(ev);
        } catch (const ValidationError& ex) {
            throw ParseError(path, r.line, ex.what());
        }
    }
    return events;
}

void writeSnapshot(std::ostream& out, const Snapshot& s) {
    out << "# Smart Elevator snapshot v1\n";
    out << "tick," << s.tick << '\n';
    out << "strategy," << s.strategy << '\n';

    out << "# elevator,id,floor,heading,stops(;),outOfService\n";
    for (const ElevatorSnapshot& e : s.elevators) {
        out << "elevator," << e.id << ',' << e.floor << ',' << toToken(e.heading) << ',';
        for (std::size_t i = 0; i < e.stops.size(); ++i) {
            if (i > 0) out << ';';
            out << e.stops[i];
        }
        out << ',' << (e.outOfService ? 1 : 0) << '\n';
    }

    out << "# onboard,elevatorId,id,kg,origin,destination,arrivalTick,boardTick\n";
    for (const ElevatorSnapshot& e : s.elevators)
        for (const Passenger& p : e.onboard)
            out << "onboard," << e.id << ',' << p.id << ',' << p.weight.kg() << ',' << p.origin << ','
                << p.destination << ',' << p.arrivalTick << ',' << p.boardTick << '\n';

    out << "# waiting,id,kg,origin,destination,arrivalTick\n";
    for (const Passenger& p : s.waiting)
        out << "waiting," << p.id << ',' << p.weight.kg() << ',' << p.origin << ',' << p.destination << ','
            << p.arrivalTick << '\n';
}

void saveSnapshot(const Snapshot& s, const std::string& path) {
    std::ostringstream text;
    writeSnapshot(text, s);

    std::ofstream out{path};
    if (!out) throw FileError("Không mở được file để ghi: " + path);
    out << text.str();
    out.close();
    if (out.fail()) throw FileError("Ghi file thất bại: " + path);
}

Snapshot loadSnapshot(const std::string& path) {
    Snapshot s;
    bool hasTick = false;
    bool hasStrategy = false;

    for (const Record& r : readRecords(path)) {
        try {
            const std::vector<std::string>& f = r.fields;
            const std::string key = toLower(f[0]);

            if (key == "tick") {
                requireFields(r, 2);
                s.tick = parseInt(f[1], "tick");
                hasTick = true;
            } else if (key == "strategy") {
                requireFields(r, 2);
                s.strategy = f[1];
                hasStrategy = true;
            } else if (key == "elevator") {
                requireFields(r, 6);
                ElevatorSnapshot e;
                e.id = parseInt(f[1], "mã thang");
                e.floor = parseInt(f[2], "tầng");
                e.heading = parseDirection(f[3]);
                for (const std::string& token : split(f[4], ';'))
                    if (!token.empty()) e.stops.push_back(parseInt(token, "điểm dừng"));
                e.outOfService = parseInt(f[5], "cờ ngừng hoạt động") != 0;
                s.elevators.push_back(e);
            } else if (key == "onboard") {
                requireFields(r, 8);
                const int elevatorId = parseInt(f[1], "mã thang");

                ElevatorSnapshot* owner = nullptr;
                for (ElevatorSnapshot& e : s.elevators)
                    if (e.id == elevatorId) owner = &e;
                if (owner == nullptr)
                    throw ValidationError("onboard tham chiếu thang " + std::to_string(elevatorId) + " chưa khai báo");

                Passenger p = parsePassenger(f, 2);
                p.boardTick = parseInt(f[7], "tick lên thang");
                owner->onboard.push_back(p);
            } else if (key == "waiting") {
                requireFields(r, 6);
                s.waiting.push_back(parsePassenger(f, 1));
            } else {
                throw ParseError(path, r.line, "từ khoá không hợp lệ: '" + f[0] + "'");
            }
        } catch (const ValidationError& ex) {
            throw ParseError(path, r.line, ex.what());
        }
    }

    if (!hasTick || !hasStrategy || s.elevators.empty())
        throw ValidationError("Snapshot thiếu dữ liệu (cần tick, strategy và ít nhất một thang)");
    return s;
}
