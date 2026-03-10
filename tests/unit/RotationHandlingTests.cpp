#include <gtest/gtest.h>
#include "../../src/display/DisplayHelpers.h"

TEST(MirrorWindow, AdjustsAspectRatioOnRotation) {
    RotationHandler handler;

    // Initial portrait orientation
    handler.onDimensionsChanged(1080, 1920);
    EXPECT_EQ(handler.current().width,  1080u);
    EXPECT_EQ(handler.current().height, 1920u);
    EXPECT_TRUE(handler.current().isPortrait);

    // Rotate to landscape
    bool callbackFired = false;
    RotationHandler::FrameDimensions captured{};
    handler.setOnChanged([&](RotationHandler::FrameDimensions d){
        callbackFired = true;
        captured = d;
    });

    handler.onDimensionsChanged(1920, 1080);
    EXPECT_TRUE(callbackFired) << "Callback should fire on dimension change";
    EXPECT_EQ(captured.width,  1920u);
    EXPECT_EQ(captured.height, 1080u);
    EXPECT_FALSE(captured.isPortrait) << "1920x1080 should be landscape";

    float aspect = (float)captured.width / captured.height;
    EXPECT_NEAR(aspect, 16.0f/9.0f, 0.01f) << "Expected 16:9 aspect ratio";
}

TEST(MirrorWindow, NoCallbackWhenDimensionsUnchanged) {
    RotationHandler handler;
    handler.onDimensionsChanged(1920, 1080);

    int callCount = 0;
    handler.setOnChanged([&](RotationHandler::FrameDimensions){ callCount++; });

    // Same dimensions — should NOT fire
    handler.onDimensionsChanged(1920, 1080);
    EXPECT_EQ(callCount, 0) << "Callback should not fire for unchanged dimensions";

    // Different dimensions — should fire
    handler.onDimensionsChanged(3840, 2160);
    EXPECT_EQ(callCount, 1);
}

TEST(MirrorWindow, PortraitDetectionCorrect) {
    RotationHandler h;
    // Portrait
    h.onDimensionsChanged(1080, 1920);
    EXPECT_TRUE(h.current().isPortrait);
    // Square — treated as landscape (not portrait since h == w)
    h.onDimensionsChanged(1080, 1080);
    EXPECT_FALSE(h.current().isPortrait); // h > w = false when equal
    // Landscape
    h.onDimensionsChanged(1920, 1080);
    EXPECT_FALSE(h.current().isPortrait);
}

TEST(MirrorWindow, HandlesVerySmallDimensions) {
    RotationHandler h;
    h.onDimensionsChanged(320, 240);
    EXPECT_EQ(h.current().width,  320u);
    EXPECT_EQ(h.current().height, 240u);
    EXPECT_FALSE(h.current().isPortrait);
}

TEST(MirrorWindow, Handles8KResolution) {
    RotationHandler h;
    h.onDimensionsChanged(7680, 4320);
    EXPECT_EQ(h.current().width,  7680u);
    EXPECT_EQ(h.current().height, 4320u);
    EXPECT_FALSE(h.current().isPortrait);
}
