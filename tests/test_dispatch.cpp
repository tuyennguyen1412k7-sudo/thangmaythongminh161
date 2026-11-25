#include "test_common.h"

void testWeight() {
    Weight a{70.5}, b{29.5};
    CHECK((a + b).kg() == 100.0);
    CHECK((a - b).kg() == 41.0);
    CHECK(b < a);
    CHECK(a == Weight{70.5});
    CHECK((Weight{100.0} * 0.9).kg() == 90.0);
    CHECK(Weight{}.kg() == 0.0);
    CHECK_THROWS(Weight{-1.0}, ValidationError);
    CHECK_THROWS(b - a, ValidationError);

    std::ostringstream os;
    os << a;
    CHECK(os.str() == "70.5 kg");
}

void testPassengerAndDirection() {
    Passenger up = makePassenger(1, 70, 0, 5);
    Passenger down = makePassenger(2, 60, 8, 3);
    CHECK(up.direction() == Direction::Up);
    CHECK(down.direction() == Direction::Down);
    CHECK(up.describe() == "#1 (70.0 kg): tầng 0 -> tầng 5");

    CHECK(parseDirection("UP") == Direction::Up);
    CHECK(parseDirection("down") == Direction::Down);
    CHECK_THROWS(parseDirection("sideways"), ValidationError);
    CHECK(toToken(Direction::None) == "none");
}

void testRequests() {
    RequestFactory f;
    std::vector<std::unique_ptr<Request>> list;
    list.push_back(f.makeHall(3, Direction::Down, 0));
    list.push_back(f.makeCar(1, 5, 0));
    list.push_back(f.makeEmergency(8, 0, "y tế"));

    CHECK(list[0]->kind() == "hall" && list[1]->kind() == "car" && list[2]->kind() == "emergency");
    CHECK(list[2]->priority() > list[0]->priority());
    CHECK(list[0]->direction() == Direction::Down && list[1]->direction() == Direction::None);
    CHECK(list[0]->id() == 1 && list[2]->id() == 3 && f.issued() == 3);
    CHECK(list[2]->describe() == "KHẨN CẤP tại tầng 8 (y tế)");

    CHECK_THROWS(f.makeHall(3, Direction::None, 0), ValidationError);
    CHECK(f.issued() == 3);
}

void testStrategies() {
    Rig rig;
    Elevator& e1 = rig.add(1, 0);
    Elevator& e2 = rig.add(2, 8);
    RequestFactory f;
    auto request = f.makeHall(7, Direction::Down, 0);

    NearestCarStrategy nearest;
    CHECK(nearest.select(*request, rig.all())->id() == 2);
    EtaStrategy eta;
    CHECK(eta.select(*request, rig.all())->id() == 2);

    e2.assign(*f.makeCar(2, 1, 0));
    e2.assign(*f.makeCar(2, 2, 0));
    LeastLoadStrategy leastLoad;
    CHECK(leastLoad.select(*request, rig.all())->id() == 1);
    CHECK(nearest.select(*request, rig.all())->id() == 2);
    CHECK(nearest.select(*request, {}) == nullptr);
    (void)e1;

    StrategyFactory factory;
    CHECK(factory.names().size() == 3);
    CHECK_THROWS(factory.create("khong-co"), ValidationError);
    factory.registerStrategy("nearest-alias", [] { return std::make_unique<NearestCarStrategy>(); });
    CHECK(factory.names().size() == 4);
    CHECK(factory.create("nearest-alias")->name() == "nearest");
}

void testStrategySwitchStrongGuarantee() {
    ElevatorController c(defaultConfig());
    CHECK(c.strategy().name() == "eta");
    c.setStrategy("nearest");
    CHECK(c.strategy().name() == "nearest");
    CHECK_THROWS(c.setStrategy("xyz"), ValidationError);
    CHECK(c.strategy().name() == "nearest");
}

void testBasicFlow() {
    ElevatorController c(defaultConfig());
    auto stats = std::make_shared<StatsCollector>();
    c.attach(stats);
    c.addPassenger(1, 70, 0, 5);
    runUntilIdle(c);

    CHECK(c.idle());
    CHECK(stats->servedPassengers() == 1);
    CHECK(stats->count(EventType::PassengerBoarded) == 1);
    CHECK(stats->totalFloorsMoved() == 5);
    CHECK(c.building().waitingPassengers() == 0);
}

void testInputValidationKeepsState() {
    ElevatorController c(defaultConfig());
    CHECK_THROWS(c.addPassenger(1, 70, 0, 99), InvalidFloorError);
    CHECK_THROWS(c.addPassenger(1, 70, 3, 3), ValidationError);
    CHECK_THROWS(c.addPassenger(1, -3, 0, 4), ValidationError);
    CHECK_THROWS(c.callElevator(9, Direction::Up), ValidationError);
    CHECK_THROWS(c.callElevator(-1, Direction::Down), ValidationError);
    CHECK_THROWS(c.callElevator(3, Direction::None), ValidationError);
    CHECK_THROWS(c.pressFloor(99, 3), ValidationError);
    CHECK_THROWS(c.pressFloor(1, 99), InvalidFloorError);
    CHECK(c.building().waitingPassengers() == 0 && c.requestLog().empty() && c.idle());
}

void testAutoPassengerId() {
    ElevatorController c(defaultConfig());
    CHECK(c.addPassenger(70, 0, 5) == 1);
    CHECK(c.addPassenger(60, 2, 0) == 2);
    c.addPassenger(10, 65, 3, 4);
    CHECK(c.addPassenger(55, 1, 2) == 11);
    CHECK_THROWS(c.addPassenger(70, 0, 99), InvalidFloorError);
    CHECK(c.addPassenger(50, 1, 3) == 12);
}

void testEmergencyPriority() {
    ElevatorController c(defaultConfig());
    auto rec = std::make_shared<Recorder>();
    c.attach(rec);
    c.setOutOfService(2);
    c.setOutOfService(3);
    c.pressFloor(1, 3);
    c.pressFloor(1, 7);
    c.tick();
    c.tick();
    c.emergencyCall(8, "y tế");
    runUntilIdle(c);

    CHECK((rec->doorOpenFloors(1) == std::vector<int>{8, 7, 3}));
    CHECK(c.requestLog().back()->kind() == "emergency");
}

void testNormalLookOrder() {
    ElevatorController c(defaultConfig());
    auto rec = std::make_shared<Recorder>();
    c.attach(rec);
    c.setOutOfService(2);
    c.setOutOfService(3);
    c.pressFloor(1, 7);
    c.pressFloor(1, 3);
    runUntilIdle(c);
    CHECK((rec->doorOpenFloors(1) == std::vector<int>{3, 7}));
}

void testPendingWhenNoElevator() {
    ElevatorController c(defaultConfig());
    c.setOutOfService(1);
    c.setOutOfService(2);
    c.setOutOfService(3);
    c.callElevator(4, Direction::Up);
    CHECK(c.pendingCount() == 1);

    c.restoreService(2);
    c.tick();
    CHECK(c.pendingCount() == 0);
    CHECK(c.elevator(2).stateName() == "Mở cửa");
}

void testQueries() {
    ElevatorController c(defaultConfig());
    c.pressFloor(1, 3);
    c.callElevator(5, Direction::Down);
    c.emergencyCall(8, "y tế");

    CHECK(c.findRequests("car").size() == 1);
    CHECK(c.findRequests("hall").size() == 1);
    CHECK(c.findRequests("emergency").size() == 1);
    CHECK(c.findRequests("khong-co").empty());

    c.elevator(1).board(makePassenger(1, 100, 0, 5));
    c.elevator(3).board(makePassenger(2, 300, 9, 0));
    const std::vector<const Elevator*> sorted = c.elevatorsByLoad();
    CHECK(sorted.size() == 3);
    CHECK(sorted[0]->id() == 3 && sorted[1]->id() == 1 && sorted[2]->id() == 2);

    ElevatorController d(defaultConfig());
    d.addPassenger(1, 70, 0, 5);
    d.tick();
    d.tick();
    d.addPassenger(2, 60, 9, 1);
    const std::vector<Passenger> waiting = d.waitingList();
    CHECK(waiting.size() >= 1);
    CHECK(waiting.front().arrivalTick <= waiting.back().arrivalTick);
}

void testSimulatorErrorsDontStopRun() {
    ElevatorController c(defaultConfig());
    auto stats = std::make_shared<StatsCollector>();
    c.attach(stats);
    const SimulationResult r = Simulator(c).run(loadScenario(kData + "/scenario_errors.csv"));

    CHECK(r.commandsOk == 1 && r.commandsFailed == 7);
    CHECK(!r.timedOut);
    CHECK(stats->servedPassengers() == 1);
}

void testAllStrategiesServeEveryone() {
    const std::vector<std::string> strategyNames{"nearest", "eta", "least-load"};
    for (const std::string& name : strategyNames) {
        SystemConfig cfg = defaultConfig();
        cfg.strategy = name;
        ElevatorController c(cfg);
        auto stats = std::make_shared<StatsCollector>();
        c.attach(stats);
        const SimulationResult r = Simulator(c).run(loadScenario(kData + "/scenario_rush.csv"));
        CHECK(!r.timedOut);
        CHECK(stats->servedPassengers() == 12);
        CHECK(c.idle());
    }
}

void testConfigValidation() {
    SystemConfig c = defaultConfig();
    c.elevators[1].startFloor = 50;
    CHECK_THROWS(ElevatorController{c}, InvalidFloorError);
    c = defaultConfig();
    c.elevators.clear();
    CHECK_THROWS(ElevatorController{c}, ValidationError);
    c = defaultConfig();
    c.strategy = "nope";
    CHECK_THROWS(ElevatorController{c}, ValidationError);
    c = defaultConfig();
    c.dwellTicks = 0;
    CHECK_THROWS(ElevatorController{c}, ValidationError);
}

void registerDispatchTests(std::vector<TestCase>& tests) {
    tests.push_back({"Weight (value type + operator)", testWeight});
    tests.push_back({"Passenger + Direction", testPassengerAndDirection});
    tests.push_back({"Request đa hình + Factory", testRequests});
    tests.push_back({"DispatchStrategy + StrategyFactory", testStrategies});
    tests.push_back({"Đổi chiến lược an toàn", testStrategySwitchStrongGuarantee});
    tests.push_back({"Luồng cơ bản", testBasicFlow});
    tests.push_back({"Kiểm tra đầu vào không làm hỏng trạng thái", testInputValidationKeepsState});
    tests.push_back({"Mã hành khách tự cấp", testAutoPassengerId});
    tests.push_back({"Ưu tiên khẩn cấp", testEmergencyPriority});
    tests.push_back({"Thứ tự LOOK", testNormalLookOrder});
    tests.push_back({"Hàng đợi khi không có thang", testPendingWhenNoElevator});
    tests.push_back({"Tra cứu / sắp xếp (STL)", testQueries});
    tests.push_back({"Simulator chịu lỗi từng lệnh", testSimulatorErrorsDontStopRun});
    tests.push_back({"Mọi chiến lược đều phục vụ hết khách", testAllStrategiesServeEveryone});
    tests.push_back({"Kiểm tra cấu hình", testConfigValidation});
}
