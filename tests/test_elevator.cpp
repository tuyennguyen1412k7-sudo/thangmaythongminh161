#include "test_common.h"

void testBuilding() {
    Building b(-1, 3);
    CHECK(b.floorCount() == 5);
    CHECK(b.floor(-1).label() == "B1");
    CHECK(b.floor(0).label() == "G");
    CHECK(b.floor(3).label() == "3");
    CHECK_THROWS(b.validate(4), InvalidFloorError);
    CHECK_THROWS(b.floor(-2), InvalidFloorError);
    CHECK_THROWS(Building(3, 3), ValidationError);

    b.floor(0).addWaiting(makePassenger(1, 70, 0, 2));
    CHECK(b.floor(0).waitingCount(Direction::Up) == 1);
    CHECK(b.floor(0).waitingCount(Direction::Down) == 0);
    CHECK(b.floor(0).removeWaiting(1));
    CHECK(!b.floor(0).removeWaiting(1));
}

void testElevatorDescribe() {
    Rig rig;
    Elevator& e = rig.add(1, 0);
    CHECK(e.describe() == "Thang 1 | tầng 0 | Chờ | hướng Đứng yên | tải 0.0 kg/800.0 kg | điểm dừng: -");
    RequestFactory f;
    e.assign(*f.makeCar(1, 5, 0));
    e.assign(*f.makeCar(1, 3, 0));
    CHECK(e.describe() == "Thang 1 | tầng 0 | Chờ | hướng Đứng yên | tải 0.0 kg/800.0 kg | điểm dừng: 3,5");
}

void testBoardStrongGuarantee() {
    Rig rig;
    Elevator& e = rig.add(1, 0, 800.0);
    e.board(makePassenger(1, 300, 0, 5));
    e.board(makePassenger(2, 300, 0, 6));

    const Weight before = e.load();
    const auto stopsBefore = e.stops();
    CHECK_THROWS(e.board(makePassenger(3, 300, 0, 7)), OverloadError);
    CHECK(e.load() == before && e.passengerCount() == 2 && e.stops() == stopsBefore);

    CHECK_THROWS(e.board(makePassenger(4, 10, 0, 99)), InvalidFloorError);
    CHECK(e.load() == before && e.passengerCount() == 2);

    e.board(makePassenger(5, 200, 0, 4));
    CHECK(e.load().kg() == 800.0);
    try {
        e.board(makePassenger(6, 1, 0, 3));
        CHECK(false);
    } catch (const OverloadError& ex) {
        CHECK(ex.limitKg() == 800.0 && ex.attemptedKg() == 801.0);
    }
}

void testStateTransitions() {
    Rig rig;
    Elevator& e = rig.add(1, 0);
    RequestFactory f;
    e.assign(*f.makeCar(1, 2, 0));
    CHECK(e.stateName() == "Chờ");
    e.tick();
    CHECK(e.stateName() == "Đang lên");
    e.tick();
    CHECK(e.floor() == 1);
    e.tick();
    CHECK(e.floor() == 2 && e.stateName() == "Mở cửa");
    CHECK(e.door().isOpen());
    e.tick();
    e.tick();
    CHECK(e.stateName() == "Chờ" && !e.door().isOpen());
}

void testElevatorUnavailable() {
    ElevatorController c(defaultConfig());
    c.setOutOfService(1);
    CHECK_THROWS(c.pressFloor(1, 3), ElevatorUnavailableError);
    CHECK(c.requestLog().empty());
    CHECK_THROWS(c.setOutOfService(1), ElevatorUnavailableError);
    CHECK(c.elevator(1).stateName() == "Ngừng hoạt động");
    c.restoreService(1);
    CHECK(c.elevator(1).isAvailable());
}

void testRecallsAreTakenOnce() {
    Rig rig;
    Elevator& e = rig.add(1, 0, 800.0);
    for (int id = 1; id <= 3; ++id) rig.building.floor(0).addWaiting(makePassenger(id, 300, 0, 5));
    RequestFactory f;
    e.assign(*f.makeHall(0, Direction::Up, 0));
    e.tick();

    CHECK(e.passengerCount() == 2);
    CHECK(rig.building.floor(0).waiting().size() == 1);

    const std::vector<HallAssignment> recalls = e.takeRecalls();
    CHECK(recalls.size() == 1 && recalls[0].floor == 0 && recalls[0].direction == Direction::Up);
    CHECK(e.takeRecalls().empty());
}

void testOverloadRecall() {
    ElevatorController c(defaultConfig());
    auto stats = std::make_shared<StatsCollector>();
    c.attach(stats);
    for (const ScenarioEvent& ev : loadScenario(kData + "/scenario_overload.csv"))
        c.addPassenger(parseInt(ev.args[0], "mã"), parseDouble(ev.args[1], "kg"), parseInt(ev.args[2], "từ"), parseInt(ev.args[3], "đến"));
    runUntilIdle(c);

    CHECK(c.idle());
    CHECK(stats->count(EventType::OverloadRejected) == 1);
    CHECK(stats->servedPassengers() == 4);
}

void testOutOfServiceReassign() {
    ElevatorController c(defaultConfig());
    auto stats = std::make_shared<StatsCollector>();
    c.attach(stats);
    Simulator simulator(c);
    const SimulationResult r = simulator.run(loadScenario(kData + "/scenario_outage.csv"));

    CHECK(!r.timedOut && r.commandsFailed == 0);
    CHECK(stats->servedPassengers() == 3);
    CHECK(c.elevator(1).isAvailable());
}

void registerElevatorTests(std::vector<TestCase>& tests) {
    tests.push_back({"Building / Floor", testBuilding});
    tests.push_back({"Elevator::describe", testElevatorDescribe});
    tests.push_back({"Elevator::board - strong guarantee", testBoardStrongGuarantee});
    tests.push_back({"Chuyển trạng thái (State)", testStateTransitions});
    tests.push_back({"Thang ngừng hoạt động", testElevatorUnavailable});
    tests.push_back({"Danh sách gọi lại thang khác", testRecallsAreTakenOnce});
    tests.push_back({"Quá tải và gọi lại", testOverloadRecall});
    tests.push_back({"Sự cố và phân lại yêu cầu", testOutOfServiceReassign});
}
