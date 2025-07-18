// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/hwaccelerated/iaccelerated.h>
#include <vespa/vespalib/hwaccelerated/hwy_impl.h>
#include <vespa/log/log.h>
LOG_SETUP("hwaccelerated_test");

using namespace vespalib;

template<typename T>
std::vector<T> createAndFill(size_t sz) {
    std::vector<T> v(sz);
    for (size_t i(0); i < sz; i++) {
        v[i] = rand()%500;
    }
    return v;
}

template<typename T, typename P>
void verifyEuclideanDistance(const hwaccelerated::IAccelerated & accel, size_t testLength, double approxFactor) {
    srand(1);
    std::vector<T> a = createAndFill<T>(testLength);
    std::vector<T> b = createAndFill<T>(testLength);
    for (size_t j(0); j < 0x20; j++) {
        P sum(0);
        for (size_t i(j); i < testLength; i++) {
            P d = P(a[i]) - P(b[i]);
            sum += d * d;
        }
        P hwComputedSum(accel.squaredEuclideanDistance(&a[j], &b[j], testLength - j));
        EXPECT_NEAR(sum, hwComputedSum, sum*approxFactor);
    }
}

void
verifyEuclideanDistance(const hwaccelerated::IAccelerated & accelerator, size_t testLength) {
    verifyEuclideanDistance<int8_t, double>(accelerator, testLength, 0.0);
    verifyEuclideanDistance<float, double>(accelerator, testLength, 0.0001); // Small deviation requiring EXPECT_APPROX
    verifyEuclideanDistance<double, double>(accelerator, testLength, 0.0);
}

TEST(HWAcceleratedTest, test_euclidean_distance) {
    constexpr size_t TEST_LENGTH = 140000; // must be longer than 64k
    GTEST_DO(verifyEuclideanDistance(*hwaccelerated::IAccelerated::create_platform_baseline_accelerator(), TEST_LENGTH));
    GTEST_DO(verifyEuclideanDistance(hwaccelerated::IAccelerated::getAccelerator(), TEST_LENGTH));
}

GTEST_MAIN_RUN_ALL_TESTS()
