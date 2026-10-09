/*!
 * \file test-ntc.c
 * \author Alessandro Fascini
 *
 * \brief NTC test file.
 */

#include "unity.h"
#include "ntc-api.h"
// Include the FFF library for function mocking (must be pasted into test/include/fff.h)
#include "fff.h"

void testNtcInit(void) {
    struct NTC ntc;
    const float r_pullup = 10000.0f; // 10k ohm pull-up resistor
    const float r0 = 10000.0f;       // 10k ohm nominal resistance at t0_kelvin
    const float t0_kelvin = 298.15f; // 25 degC in kelvin
    const float beta = 3950.0f;      // Beta coefficient
    const float vcc_mv = 3300.0f;    // Supply voltage in millivolts

    enum NtcError result = ntc_init(&ntc, r_pullup, r0, t0_kelvin, beta, vcc_mv);
    TEST_ASSERT_EQUAL(NTC_OK, result);
    TEST_ASSERT_EQUAL_FLOAT(r_pullup, ntc.r_pullup);
    TEST_ASSERT_EQUAL_FLOAT(r0, ntc.r0);
    TEST_ASSERT_EQUAL_FLOAT(t0_kelvin, ntc.t0_kelvin);
    TEST_ASSERT_EQUAL_FLOAT(beta, ntc.beta);
    TEST_ASSERT_EQUAL_FLOAT(vcc_mv, ntc.vcc_mv);

    // Test invalid parameters
    TEST_ASSERT_EQUAL(NTC_SELF_IS_NULL, ntc_init(NULL, 0, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_R_PULLUP, ntc_init(&ntc, 0, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_R0, ntc_init(&ntc, r_pullup, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_T0, ntc_init(&ntc, r_pullup, r0, 0, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_BETA, ntc_init(&ntc, r_pullup, r0, t0_kelvin, 0, 0));
    TEST_ASSERT_EQUAL(NTC_INVALID_VCC, ntc_init(&ntc, r_pullup, r0, t0_kelvin, beta, 0));
}

void testNtcAddSample(void) {
    // TODO: implement - cover that ntc_add_sample() delegates correctly into self->filter
    // (reuse the same below/at/above-capacity scenarios as AdcMovingAverage's add_sample test)
}

void testNtcRawAdcToResistance(void) {
    // TODO: implement - cover known raw_adc -> resistance conversions against hand-computed
    // expected values, using the circuit's r_pullup/vcc_mv
}

void testNtcResistanceToCelsius(void) {
    // TODO: implement - cover known resistance -> celsius conversions against hand-computed
    // expected values using the Beta equation (r0/t0_kelvin/beta), including resistance == r0
    // (should return the nominal temperature)
}

void testNtcGetCelsius(void) {
    // TODO: implement - cover the no-data (NAN) case and a celsius reading after adding samples
}

void testNtcGetLastCelsius(void) {
    // TODO: implement - cover the no-data (NAN) case and the last raw sample converted to celsius
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(testNtcInit);
    RUN_TEST(testNtcAddSample);
    RUN_TEST(testNtcRawAdcToResistance);
    RUN_TEST(testNtcResistanceToCelsius);
    RUN_TEST(testNtcGetCelsius);
    RUN_TEST(testNtcGetLastCelsius);
    return UNITY_END();
}
