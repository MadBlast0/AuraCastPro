// DeviceManagerTests.cpp — Unit tests for DeviceManager
// Task 204
#include <gtest/gtest.h>
#include "../../src/manager/DeviceManager.h"

using namespace aura;

class DeviceManagerTest : public ::testing::Test {
protected:
    DeviceManager dm;
    void SetUp() override { dm.init(); }
    void TearDown() override { dm.shutdown(); }
};

TEST_F(DeviceManagerTest, InitStartsEmpty) {
    EXPECT_EQ(dm.deviceCount(), 0);
    EXPECT_TRUE(dm.allDevices().empty());
}

TEST_F(DeviceManagerTest, AddDevice) {
    DeviceInfo dev;
    dev.id = "test-001";
    dev.name = "Test iPhone";
    dev.protocol = DeviceProtocol::AirPlay;
    dev.state = DeviceState::Discovered;
    dm.addOrUpdateDevice(dev);
    EXPECT_EQ(dm.deviceCount(), 1);
}

TEST_F(DeviceManagerTest, FindByIdReturnsCorrectDevice) {
    DeviceInfo dev;
    dev.id = "findme-123";
    dev.name = "Find Me";
    dev.protocol = DeviceProtocol::AirPlay;
    dev.state = DeviceState::Discovered;
    dm.addOrUpdateDevice(dev);
    auto found = dm.findById("findme-123");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Find Me");
}

TEST_F(DeviceManagerTest, FindByIdReturnNullForUnknown) {
    auto found = dm.findById("does-not-exist");
    EXPECT_FALSE(found.has_value());
}

TEST_F(DeviceManagerTest, UpdateDeviceStateChangesState) {
    DeviceInfo dev;
    dev.id = "state-test";
    dev.name = "State Device";
    dev.protocol = DeviceProtocol::Cast;
    dev.state = DeviceState::Discovered;
    dm.addOrUpdateDevice(dev);
    dm.updateDeviceState("state-test", DeviceState::Connected);
    auto found = dm.findById("state-test");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->state, DeviceState::Connected);
}

TEST_F(DeviceManagerTest, RemoveDevice) {
    DeviceInfo dev;
    dev.id = "remove-me";
    dev.name = "Remove Me";
    dev.protocol = DeviceProtocol::ADB;
    dev.state = DeviceState::Discovered;
    dm.addOrUpdateDevice(dev);
    EXPECT_EQ(dm.deviceCount(), 1);
    dm.removeDevice("remove-me");
    EXPECT_EQ(dm.deviceCount(), 0);
}

TEST_F(DeviceManagerTest, CallbackFiredOnAdd) {
    bool callbackFired = false;
    dm.setDeviceChangedCallback([&](const DeviceInfo&) { callbackFired = true; });
    DeviceInfo dev;
    dev.id = "cb-test"; dev.name = "CB"; dev.protocol = DeviceProtocol::AirPlay;
    dev.state = DeviceState::Discovered;
    dm.addOrUpdateDevice(dev);
    EXPECT_TRUE(callbackFired);
}
