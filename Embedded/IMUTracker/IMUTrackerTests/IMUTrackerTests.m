#import <XCTest/XCTest.h>
#import "Q30.h"
#import "Quaternion.h"

static uint32_t multiplyQ30_acc(int32_t a, int32_t b)
{
    return (uint32_t)(((int64_t)a * (int64_t)b) >> 30);
}

@interface IMUTrackerTests : XCTestCase

@end

@implementation IMUTrackerTests

- (void)testConvertToFloat
{
    XCTAssertEqual(convertQ30ToFloat(1 << 30), 1.0);
    XCTAssertEqual(convertQ30ToFloat(1 << 29), 0.5);
    XCTAssertEqual(convertQ30ToFloat(1), powf(2, -30));
    XCTAssertEqual(convertQ30ToFloat(0b111 << 28), 1.75);
    XCTAssertEqual(convertQ30ToFloat((1 << 31) | (1 << 28)), -1.75);
    XCTAssertEqual(convertQ30ToFloat(0), 0.0);
    XCTAssertEqual(convertQ30ToFloat(0b111 << 29), -0.5);
    XCTAssertEqual(convertQ30ToFloat(0b1111 << 28), -0.25);
}

- (void)testConvertToQ30
{
    XCTAssertEqual(convertFloatToQ30(1.0), 1 << 30);
    XCTAssertEqual(convertFloatToQ30(0.5), 1 << 29);
    XCTAssertEqual(convertFloatToQ30(powf(2, -30)), 1);
    XCTAssertEqual(convertFloatToQ30(1.75), 0b111 << 28);
    XCTAssertEqual(convertFloatToQ30(-1.75), (1 << 31) | (1 << 28));
    XCTAssertEqual(convertFloatToQ30(0.0), 0);
    XCTAssertEqual(convertFloatToQ30(-0.5), 0b111 << 29);
    XCTAssertEqual(convertFloatToQ30(-0.25), 0b1111 << 28);
}

- (void)testMultiply
{
    XCTAssertEqual(multiplyQ30(0, 0), 0);
    XCTAssertEqual(multiplyQ30(1 << 29, 1 << 29), 1 << 28); /* 0.5 * 0.5 = 0.25 */
    XCTAssertEqual(multiplyQ30(0b111 << 29, 1 << 29), 0b1111 << 28); /* -0.5 * 0.5 = -0.25 */
    XCTAssertEqual(multiplyQ30(1 << 29, 0b111 << 29), 0b1111 << 28); /* 0.5 * -0.5 = -0.25 */
    XCTAssertEqual(multiplyQ30(0b111 << 29, 0b111 << 29), 1 << 28); /* -0.5 * -0.5 = 0.25 */
    for (int i = 0; i < 100; ++i) {
        float a = (float)arc4random() / UINT32_MAX;
        float b = (float)arc4random() / UINT32_MAX;
        if (arc4random_uniform(2)) {
            a *= -1;
        }
        if (arc4random_uniform(2)) {
            b *= -1;
        }
        uint32_t qa = convertFloatToQ30(a);
        uint32_t qb = convertFloatToQ30(b);
        uint32_t qProd = multiplyQ30(qa, qb);
        XCTAssertEqualWithAccuracy(convertQ30ToFloat(qProd), a * b, powf(2, -24));
        XCTAssertEqualWithAccuracy(qProd, multiplyQ30_acc(qa, qb), 1 << 3);
        XCTAssertEqualWithAccuracy(convertQ30ToFloat(squareQ30(qa)), a * a, powf(2, -24));
    }
}

- (void)testSqrt
{
    for (int i = 0; i < 100; ++i) {
        float x = (float)arc4random() / UINT32_MAX;
        uint32_t qx = convertFloatToQ30(x);
        XCTAssertEqualWithAccuracy(convertQ30ToFloat(sqrtQ30(qx)), sqrtf(x), 2e-4);
    }
}

- (void)testInverse
{
    for (int i = 0; i < 100; ++i) {
        const float theta = (float)arc4random() / UINT32_MAX * 2 * M_PI - M_PI;
        const float ux = (float)arc4random() / UINT32_MAX - 0.5;
        const float uy = (float)arc4random() / UINT32_MAX - 0.5;
        const float uz = sqrtf(1 - ux * ux - uy * uy);
        
        const int32_t w = convertFloatToQ30(cosf(theta / 2));
        const int32_t x = convertFloatToQ30(ux * sinf(theta / 2));
        const int32_t y = convertFloatToQ30(uy * sinf(theta / 2));
        const int32_t z = convertFloatToQ30(uz * sinf(theta / 2));
        quaternion_t quat = {.w.value = w, .x.value = x, .y.value = y, .z.value = z};
        quaternion_t inv;
        quaternion_inverse(&quat, &inv);
        quaternion_t prod;
        quaternion_multiply(&quat, &inv, &prod);
        XCTAssertEqualWithAccuracy(convertQ30ToFloat(prod.w.value), 1.0, 2e-4);
        XCTAssertEqualWithAccuracy(convertQ30ToFloat(prod.x.value), 0.0, 2e-4);
        XCTAssertEqualWithAccuracy(convertQ30ToFloat(prod.y.value), 0.0, 2e-4);
        XCTAssertEqualWithAccuracy(convertQ30ToFloat(prod.z.value), 0.0, 2e-4);
    }
}

- (void)testCLZ
{
    XCTAssertEqual(count_leading_zeros(0), 32);
    XCTAssertEqual(count_leading_zeros(0xFFFFFFFF), __builtin_clz(0xFFFFFFFF));
    for (int i = 0; i < 1000; ++i) {
        uint32_t x = arc4random();
        while (x == 0) x = arc4random();
        XCTAssertEqual(count_leading_zeros(x), __builtin_clz(x));
    }
}

@end
