// Copyright 2021 GHA Test Team
#include "TimedDoor.h"
#include <thread>
#include <chrono>
#include <stdexcept>
#include <unordered_map>
#include <functional>

std::function<void(int, TimerClient*)> g_registerTimer;

static std::unordered_map<TimedDoor*, bool> armedMap;

void Timer::sleep(int seconds) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

void Timer::tregister(int timeout, TimerClient* client) {
    if (g_registerTimer) {
        g_registerTimer(timeout, client);
    } else {
        if (timeout == 0) {
            if (client) client->Timeout();
        } else {
            std::thread([timeout, client]() {
                std::this_thread::sleep_for(std::chrono::seconds(timeout));
                if (client) client->Timeout();
            }).detach();
        }
    }
}

DoorTimerAdapter::DoorTimerAdapter(TimedDoor& d) : door(d) {
    armedMap[&door] = false;
}

void DoorTimerAdapter::Timeout() {
    bool& armed = armedMap[&door];
    if (!armed) {
        armed = true;
        Timer timer;
        timer.tregister(door.getTimeOut(), this);
    } else {
        if (door.isDoorOpened()) {
            door.throwState();
        }
        armed = false;
    }
}

TimedDoor::TimedDoor(int timeout) : iTimeout(timeout), isOpened(false) {
    adapter = new DoorTimerAdapter(*this);
}

TimedDoor::~TimedDoor() {
    delete adapter;
}

bool TimedDoor::isDoorOpened() {
    return isOpened;
}

void TimedDoor::unlock() {
    isOpened = true;
    adapter->Timeout();
}

void TimedDoor::lock() {
    isOpened = false;
}

int TimedDoor::getTimeOut() const {
    return iTimeout;
}

void TimedDoor::throwState() {
    throw std::runtime_error("Door is open!");
}
