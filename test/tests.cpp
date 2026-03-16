// Copyright 2021 GHA Test Team
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <functional>
#include "TimedDoor.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::MockFunction;

extern std::function<void(int, TimerClient*)> g_registerTimer;

class MockTimedDoor : public TimedDoor {
 public:
    explicit MockTimedDoor(int t) : TimedDoor(t) {}
    MOCK_METHOD(void, throwState, (), (override));
    MOCK_METHOD(bool, isDoorOpened, (), (override));
    MOCK_METHOD(int, getTimeOut, (), (const, override));
};

class MockTimerClient : public TimerClient {
 public:
    MOCK_METHOD(void, Timeout, (), (override));
};

class RegisterTimerGuard {
 public:
    explicit RegisterTimerGuard(std::function<void(int, TimerClient*)> func) {
        oldFunc = g_registerTimer;
        g_registerTimer = func;
    }
    ~RegisterTimerGuard() {
        g_registerTimer = oldFunc;
    }
 private:
    std::function<void(int, TimerClient*)> oldFunc;
};

TEST(TimedDoorTest, InitialStateIsClosed) {
    TimedDoor door(78);
    EXPECT_FALSE(door.isDoorOpened());
    EXPECT_EQ(door.getTimeOut(), 78);
}

TEST(TimedDoorTest, UnlockOpensDoor) {
    TimedDoor door(89);
    door.unlock();
    EXPECT_TRUE(door.isDoorOpened());
}

TEST(TimedDoorTest, LockClosesDoor) {
    TimedDoor door(7);
    door.unlock();
    door.lock();
    EXPECT_FALSE(door.isDoorOpened());
}

TEST(TimedDoorTest, GetTimeOutReturnsCorrectValue) {
    TimedDoor door(90);
    EXPECT_EQ(door.getTimeOut(), 90);
}

TEST(TimedDoorTest, ThrowStateThrowsException) {
    TimedDoor door(8);
    EXPECT_THROW(door.throwState(), std::runtime_error);
}

TEST(DoorTimerAdapterTest, TimeoutWhenDoorOpenCallsThrowState) {
    MockTimedDoor mockDoor(7);
    EXPECT_CALL(mockDoor, getTimeOut()).WillOnce(Return(7));
    EXPECT_CALL(mockDoor, isDoorOpened()).WillOnce(Return(true));
    EXPECT_CALL(mockDoor, throwState()).Times(1);

    auto immediate = [](int, TimerClient* client) {
        client->Timeout();
    };
    RegisterTimerGuard guard(immediate);

    DoorTimerAdapter adapter(mockDoor);
    adapter.Timeout();
}

TEST(DoorTimerAdapterTest, TimeoutWhenDoorClosedDoesNothing) {
    MockTimedDoor mockDoor(7);
    EXPECT_CALL(mockDoor, getTimeOut()).WillOnce(Return(7));
    EXPECT_CALL(mockDoor, isDoorOpened()).WillOnce(Return(false));
    EXPECT_CALL(mockDoor, throwState()).Times(0);

    auto immediate = [](int, TimerClient* client) {
        client->Timeout();
    };
    RegisterTimerGuard guard(immediate);

    DoorTimerAdapter adapter(mockDoor);
    adapter.Timeout();
}

TEST(DoorTimerAdapterTest, FirstTimeoutRegistersTimerWithCorrectTime) {
    MockTimedDoor mockDoor(8);
    EXPECT_CALL(mockDoor, getTimeOut()).WillOnce(Return(8));

    MockFunction<void(int, TimerClient*)> mockRegister;
    EXPECT_CALL(mockRegister, Call(8, _)).Times(1);

    auto recorder = [&mockRegister](int timeout, TimerClient* client) {
        mockRegister.Call(timeout, client);
    };
    RegisterTimerGuard guard(recorder);

    DoorTimerAdapter adapter(mockDoor);
    adapter.Timeout();
}

TEST(DoorTimerAdapterTest, SecondTimeoutDoesNotRegisterAgain) {
    MockTimedDoor mockDoor(8);
    EXPECT_CALL(mockDoor, getTimeOut()).Times(1).WillOnce(Return(8));
    EXPECT_CALL(mockDoor, isDoorOpened()).WillOnce(Return(false));
    EXPECT_CALL(mockDoor, throwState()).Times(0);

    MockFunction<void(int, TimerClient*)> mockRegister;
    EXPECT_CALL(mockRegister, Call(8, _)).Times(1);

    auto recorder = [&mockRegister](int timeout, TimerClient* client) {
        mockRegister.Call(timeout, client);
    };
    RegisterTimerGuard guard(recorder);

    DoorTimerAdapter adapter(mockDoor);
    adapter.Timeout();
    adapter.Timeout();
}

TEST(DoorTimerAdapterTest, MultipleCyclesWorkCorrectly) {
    MockTimedDoor mockDoor(9);
    EXPECT_CALL(mockDoor, getTimeOut())
        .Times(2)
        .WillRepeatedly(Return(9));
    EXPECT_CALL(mockDoor, isDoorOpened())
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    EXPECT_CALL(mockDoor, throwState()).Times(1);

    auto immediate = [](int, TimerClient* client) {
        client->Timeout();
    };
    RegisterTimerGuard guard(immediate);

    DoorTimerAdapter adapter(mockDoor);
    adapter.Timeout();
    adapter.Timeout();
}

TEST(TimerTest, RegisterWithZeroTimeoutCallsImmediately) {
    MockTimerClient mockClient;
    EXPECT_CALL(mockClient, Timeout()).Times(1);

    Timer timer;
    timer.tregister(0, &mockClient);
}

TEST(TimedDoorTest, GetTimeOutUnchangedAfterOperations) {
    TimedDoor door(78);
    EXPECT_EQ(door.getTimeOut(), 78);
    door.unlock();
    EXPECT_EQ(door.getTimeOut(), 78);
    door.lock();
    EXPECT_EQ(door.getTimeOut(), 78);
}

TEST(TimedDoorTest, MultipleDoorsIndependent) {
    TimedDoor door1(7);
    TimedDoor door2(8);
    EXPECT_EQ(door1.getTimeOut(), 7);
    EXPECT_EQ(door2.getTimeOut(), 8);
    door1.unlock();
    door2.unlock();
    EXPECT_TRUE(door1.isDoorOpened());
    EXPECT_TRUE(door2.isDoorOpened());
    door1.lock();
    EXPECT_FALSE(door1.isDoorOpened());
    EXPECT_TRUE(door2.isDoorOpened());
}

TEST(DoorTimerAdapterTest, MultipleAdaptersIndependent) {
    MockTimedDoor mockDoor1(7);
    MockTimedDoor mockDoor2(9);

    EXPECT_CALL(mockDoor1, getTimeOut()).WillOnce(Return(7));
    EXPECT_CALL(mockDoor1, isDoorOpened()).WillOnce(Return(false));
    EXPECT_CALL(mockDoor1, throwState()).Times(0);

    EXPECT_CALL(mockDoor2, getTimeOut()).WillOnce(Return(9));
    EXPECT_CALL(mockDoor2, isDoorOpened()).WillOnce(Return(true));
    EXPECT_CALL(mockDoor2, throwState()).Times(1);

    auto immediate = [](int, TimerClient* client) {
        client->Timeout();
    };
    RegisterTimerGuard guard(immediate);

    DoorTimerAdapter adapter1(mockDoor1);
    DoorTimerAdapter adapter2(mockDoor2);

    adapter1.Timeout();
    adapter2.Timeout();
}

TEST(DoorTimerAdapterTest, DoorClosedBeforeTimeoutNoException) {
    MockTimedDoor mockDoor(7);
    EXPECT_CALL(mockDoor, getTimeOut()).WillOnce(Return(7));
    EXPECT_CALL(mockDoor, isDoorOpened()).WillOnce(Return(false));
    EXPECT_CALL(mockDoor, throwState()).Times(0);

    TimerClient* savedClient = nullptr;
    auto registerFunc = [&savedClient](int, TimerClient* client) {
        savedClient = client;
    };
    RegisterTimerGuard guard(registerFunc);

    DoorTimerAdapter adapter(mockDoor);
    adapter.Timeout();

    savedClient->Timeout();
}

TEST(IntegrationTest, UnlockWithZeroTimeoutThrows) {
    TimedDoor door(0);
    door.lock();
    EXPECT_THROW(door.unlock(), std::runtime_error);
}

TEST(IntegrationTest, UnlockWithPositiveTimeoutDoesNotThrowImmediately) {
    TimedDoor door(7);
    door.lock();
    EXPECT_NO_THROW(door.unlock());
    EXPECT_TRUE(door.isDoorOpened());
}

TEST(TimedDoorTest, DestructorWorks) {
    auto door = new TimedDoor(9);
    delete door;
    SUCCEED();
}
