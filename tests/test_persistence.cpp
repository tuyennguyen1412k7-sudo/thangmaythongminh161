#include "test_common.h"

void testSnapshotRoundTrip() {
    ElevatorController c(defaultConfig());
    c.addPassenger(1, 70, 0, 8);
    c.addPassenger(2, 60, 6, 1);
    for (int i = 0; i < 3; ++i) c.tick();

    const Snapshot s = c.snapshot();
    const std::string path = "test_snapshot.tmp.csv";
    saveSnapshot(s, path);
    
    const Snapshot r = loadSnapshot(path);
    CHECK(r.tick == s.tick && r.strategy == s.strategy && r.elevators.size() == s.elevators.size());
    CHECK(r.waiting.size() == s.waiting.size());
    for (std::size_t i = 0; i < s.elevators.size(); ++i) {
        CHECK(r.elevators[i].floor == s.elevators[i].floor);
        CHECK(r.elevators[i].stops == s.elevators[i].stops);
        CHECK(r.elevators[i].onboard.size() == s.elevators[i].onboard.size());
    }

    ElevatorController c2(defaultConfig());
    auto stats = std::make_shared<StatsCollector>();
    c2.attach(stats);
    c2.restore(r);
    CHECK(c2.now() == s.tick);
    runUntilIdle(c2);
    CHECK(c2.idle());
    CHECK(stats->servedPassengers() >= 1);
}

void testRestoreIsAtomic() {
    ElevatorController c(defaultConfig());
    c.addPassenger(1, 70, 0, 8);
    c.tick();
    c.tick();
    const Snapshot before = c.snapshot();

    Snapshot bad = before;
    bad.elevators.back().floor = 42;
    CHECK_THROWS(c.restore(bad), InvalidFloorError);

    Snapshot badStrategy = before;
    badStrategy.strategy = "xyz";
    CHECK_THROWS(c.restore(badStrategy), ValidationError);

    Snapshot tooHeavy = before;
    tooHeavy.elevators[0].onboard.push_back(makePassenger(9, 900, 0, 3));
    CHECK_THROWS(c.restore(tooHeavy), OverloadError);

    const Snapshot after = c.snapshot();
    CHECK(after.tick == before.tick && after.strategy == before.strategy);
    for (std::size_t i = 0; i < before.elevators.size(); ++i) {
        CHECK(after.elevators[i].floor == before.elevators[i].floor);
        CHECK(after.elevators[i].stops == before.elevators[i].stops);
    }
}

void testFiles() {
    CHECK_THROWS(loadConfig(kData + "/khong_co.csv"), FileError);
    CHECK_THROWS(loadScenario(kData + "/scenario_bad.csv"), ParseError);
    try {
        loadScenario(kData + "/scenario_bad.csv");
    } catch (const ParseError& e) {
        CHECK(e.line() == 3);
    }

    const SystemConfig cfg = loadConfig(kData + "/config_default.csv");
    CHECK(cfg.elevators.size() == 3 && cfg.lowestFloor == -1 && cfg.highestFloor == 9);

    const std::string bad = "test_bad_config.tmp.csv";
    {
        std::ofstream(bad) << "building,0,9\nelevator,1,0,800\nelevator,1,3,800\n";
    }
    CHECK_THROWS(loadConfig(bad), ValidationError);
    {
        std::ofstream(bad) << "building,0,9\nfoo,1\n";
    }
    CHECK_THROWS(loadConfig(bad), ParseError);
    {
        std::ofstream(bad) << "tick,1\nstrategy,eta\nelevator,1,0,none,,0\nonboard,7,1,70,0,3,0,0\n";
    }
    CHECK_THROWS(loadSnapshot(bad), ParseError);

    CHECK_THROWS(saveSnapshot(Snapshot{}, "khong/co/thu/muc/x.csv"), FileError);
}

void registerPersistenceTests(std::vector<TestCase>& tests) {
    tests.push_back({"Snapshot lưu/đọc/khôi phục", testSnapshotRoundTrip});
    tests.push_back({"Khôi phục nguyên tử", testRestoreIsAtomic});
    tests.push_back({"Lỗi file", testFiles});
}
