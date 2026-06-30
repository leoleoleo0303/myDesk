#if defined(__APPLE__)

#include "core/capture/capture_mac.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

namespace rd {

MacScreenCapture::~MacScreenCapture() {
    shutdown();
}

bool MacScreenCapture::init() {
    // No persistent state needed for CGWindowListCreateImage approach.
    // Initialization succeeds if we're running on macOS.
    return true;
}

bool MacScreenCapture::captureFrame(Frame& out) {
    // Capture the entire main display
    CGImageRef imageRef = CGWindowListCreateImage(
        CGRectInfinite,
        kCGWindowListOptionOnScreenOnly,
        kCGNullWindowID,
        kCGWindowImageDefault
    );

    if (!imageRef) {
        return false;
    }

    const int width = static_cast<int>(CGImageGetWidth(imageRef));
    const int height = static_cast<int>(CGImageGetHeight(imageRef));

    if (width <= 0 || height <= 0) {
        CGImageRelease(imageRef);
        return false;
    }

    // Create a bitmap context with BGRA pixel format
    const int bytesPerRow = width * Frame::bytesPerPixel();
    std::vector<uint8_t> buffer(static_cast<size_t>(bytesPerRow) * height);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (!colorSpace) {
        CGImageRelease(imageRef);
        return false;
    }

    // kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little produces BGRA layout
    CGContextRef context = CGBitmapContextCreate(
        buffer.data(),
        static_cast<size_t>(width),
        static_cast<size_t>(height),
        8,                              // bits per component
        static_cast<size_t>(bytesPerRow),
        colorSpace,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
    );

    CGColorSpaceRelease(colorSpace);

    if (!context) {
        CGImageRelease(imageRef);
        return false;
    }

    // Draw the captured image into our BGRA buffer
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), imageRef);
    CGContextRelease(context);
    CGImageRelease(imageRef);

    // Fill the output frame
    out.width = width;
    out.height = height;
    out.stride = bytesPerRow;
    out.format = PixelFormat::BGRA;
    out.data = std::move(buffer);

    return true;
}

void MacScreenCapture::shutdown() {
    // No persistent resources to release with the CGWindowListCreateImage approach.
}

// Factory method implementation for macOS
std::unique_ptr<ScreenCapture> ScreenCapture::create() {
    return std::make_unique<MacScreenCapture>();
}

} // namespace rd

#endif // defined(__APPLE__)
