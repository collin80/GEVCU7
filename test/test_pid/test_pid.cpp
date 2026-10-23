/*
 * PID controller (Brett Beauregard's Arduino PID v1.2.1) used by the
 * heating/cooling and other closed loop controls. Time is virtual, so each
 * test steps millis() explicitly to cross the sample time boundary.
 */
#include "gevcu_test.h"
#include "PID_v1.h"

static double input, output, setpoint;

void setUp()
{
    hostResetAll();
    hostSetMillis(10000);
    input = output = setpoint = 0.0;
}
void tearDown() {}

void test_manual_mode_does_not_compute()
{
    PID pid(&input, &output, &setpoint, 2.0, 0.0, 0.0, DIRECT);
    setpoint = 10.0;
    TEST_ASSERT_EQUAL_INT(MANUAL, pid.GetMode());
    TEST_ASSERT_FALSE(pid.Compute());
    TEST_ASSERT_EQUAL_DOUBLE(0.0, output);
}

void test_first_compute_runs_immediately_after_enabling()
{
    PID pid(&input, &output, &setpoint, 2.0, 0.0, 0.0, DIRECT);
    pid.SetMode(AUTOMATIC);
    setpoint = 10.0;
    TEST_ASSERT_TRUE(pid.Compute());
    TEST_ASSERT_EQUAL_DOUBLE(20.0, output); // kp * error
}

void test_respects_sample_time()
{
    PID pid(&input, &output, &setpoint, 1.0, 0.0, 0.0, DIRECT);
    pid.SetMode(AUTOMATIC);
    setpoint = 5.0;
    TEST_ASSERT_TRUE(pid.Compute());
    hostAdvanceMillis(99);
    TEST_ASSERT_FALSE(pid.Compute());
    hostAdvanceMillis(1);
    TEST_ASSERT_TRUE(pid.Compute());
}

void test_output_is_clamped_to_default_limits()
{
    PID pid(&input, &output, &setpoint, 100.0, 0.0, 0.0, DIRECT);
    pid.SetMode(AUTOMATIC);
    setpoint = 10.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(255.0, output);
    setpoint = -10.0;
    hostAdvanceMillis(100);
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, output);
}

void test_custom_output_limits()
{
    PID pid(&input, &output, &setpoint, 100.0, 0.0, 0.0, DIRECT);
    pid.SetOutputLimits(-50.0, 50.0);
    pid.SetMode(AUTOMATIC);
    setpoint = -10.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(-50.0, output);
}

void test_invalid_output_limits_are_ignored()
{
    PID pid(&input, &output, &setpoint, 100.0, 0.0, 0.0, DIRECT);
    pid.SetOutputLimits(10.0, 10.0);
    pid.SetOutputLimits(20.0, 5.0);
    pid.SetMode(AUTOMATIC);
    setpoint = 10.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(255.0, output); // still the 0..255 default
}

void test_shrinking_limits_in_auto_clamps_existing_output()
{
    PID pid(&input, &output, &setpoint, 100.0, 0.0, 0.0, DIRECT);
    pid.SetMode(AUTOMATIC);
    setpoint = 10.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(255.0, output);
    pid.SetOutputLimits(0.0, 100.0);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, output);
}

void test_integral_accumulates_per_sample()
{
    // ki is scaled by the 100ms sample time: 10/s * 0.1s = 1 per sample per unit error
    PID pid(&input, &output, &setpoint, 0.0, 10.0, 0.0, DIRECT);
    pid.SetMode(AUTOMATIC);
    setpoint = 2.0;
    for (int i = 1; i <= 5; i++)
    {
        TEST_ASSERT_TRUE(pid.Compute());
        TEST_ASSERT_EQUAL_DOUBLE(2.0 * i, output);
        hostAdvanceMillis(100);
    }
}

void test_integral_windup_is_limited()
{
    PID pid(&input, &output, &setpoint, 0.0, 100.0, 0.0, DIRECT);
    pid.SetOutputLimits(0, 50);
    pid.SetMode(AUTOMATIC);
    setpoint = 100.0;
    for (int i = 0; i < 20; i++) { pid.Compute(); hostAdvanceMillis(100); }
    TEST_ASSERT_EQUAL_DOUBLE(50.0, output);
    // Error reverses: a non-wound-up integrator responds on the very next sample.
    setpoint = 0.0;
    input = 1.0;
    pid.Compute();
    TEST_ASSERT_TRUE(output < 50.0);
}

void test_derivative_on_measurement()
{
    // kd is divided by sample time: 0.5 / 0.1s = 5 per unit change of input
    PID pid(&input, &output, &setpoint, 0.0, 0.0, 0.5, DIRECT);
    pid.SetOutputLimits(-1000, 1000);
    pid.SetMode(AUTOMATIC);
    pid.Compute();
    hostAdvanceMillis(100);
    input = 4.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(-20.0, output);
    // A setpoint change alone causes no derivative kick.
    hostAdvanceMillis(100);
    setpoint = 50.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, output);
}

void test_reverse_direction_inverts_response()
{
    PID pid(&input, &output, &setpoint, 2.0, 0.0, 0.0, REVERSE);
    pid.SetOutputLimits(-100, 100);
    pid.SetMode(AUTOMATIC);
    setpoint = 10.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(-20.0, output);
    TEST_ASSERT_EQUAL_INT(REVERSE, pid.GetDirection());
}

void test_switching_direction_while_running()
{
    PID pid(&input, &output, &setpoint, 2.0, 0.0, 0.0, DIRECT);
    pid.SetOutputLimits(-100, 100);
    pid.SetMode(AUTOMATIC);
    pid.SetControllerDirection(REVERSE);
    setpoint = 10.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(-20.0, output);
}

void test_negative_tunings_rejected()
{
    PID pid(&input, &output, &setpoint, 1.0, 2.0, 3.0, DIRECT);
    pid.SetTunings(-1.0, 5.0, 5.0);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, pid.GetKp());
    TEST_ASSERT_EQUAL_DOUBLE(2.0, pid.GetKi());
    TEST_ASSERT_EQUAL_DOUBLE(3.0, pid.GetKd());
    pid.SetTunings(4.0, 5.0, 6.0);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, pid.GetKp());
    TEST_ASSERT_EQUAL_DOUBLE(5.0, pid.GetKi());
    TEST_ASSERT_EQUAL_DOUBLE(6.0, pid.GetKd());
}

void test_sample_time_change_rescales_integral_gain()
{
    PID pid(&input, &output, &setpoint, 0.0, 10.0, 0.0, DIRECT);
    pid.SetSampleTime(500);
    pid.SetMode(AUTOMATIC);
    setpoint = 1.0;
    // The constructor back-dates lastTime by the old 100ms period only.
    TEST_ASSERT_FALSE(pid.Compute());
    hostAdvanceMillis(400);
    TEST_ASSERT_TRUE(pid.Compute());
    TEST_ASSERT_EQUAL_DOUBLE(5.0, output); // 10/s * 0.5s
    hostAdvanceMillis(499);
    TEST_ASSERT_FALSE(pid.Compute());
    pid.SetSampleTime(0); // ignored
    hostAdvanceMillis(1);
    TEST_ASSERT_TRUE(pid.Compute());
}

void test_bumpless_transfer_from_manual()
{
    PID pid(&input, &output, &setpoint, 0.0, 1.0, 0.0, DIRECT);
    output = 42.0;  // value the operator was driving manually
    input = 3.0;
    setpoint = 3.0; // no error
    pid.SetMode(AUTOMATIC);
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(42.0, output);
}

void test_proportional_on_measurement()
{
    PID pid(&input, &output, &setpoint, 2.0, 0.0, 0.0, P_ON_M, DIRECT);
    pid.SetOutputLimits(-100, 100);
    pid.SetMode(AUTOMATIC);
    setpoint = 10.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, output); // no input movement yet => no P action
    hostAdvanceMillis(100);
    input = 1.0;
    pid.Compute();
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, output);
}

void test_millis_rollover()
{
    hostSetMillis(0xFFFFFFFFu - 50);
    PID pid(&input, &output, &setpoint, 0.0, 10.0, 0.0, DIRECT);
    pid.SetMode(AUTOMATIC);
    setpoint = 1.0;
    TEST_ASSERT_TRUE(pid.Compute());
    hostSetMillis(49); // 100ms later, across the wrap
    TEST_ASSERT_TRUE(pid.Compute());
    TEST_ASSERT_EQUAL_DOUBLE(2.0, output);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_manual_mode_does_not_compute);
    RUN_TEST(test_first_compute_runs_immediately_after_enabling);
    RUN_TEST(test_respects_sample_time);
    RUN_TEST(test_output_is_clamped_to_default_limits);
    RUN_TEST(test_custom_output_limits);
    RUN_TEST(test_invalid_output_limits_are_ignored);
    RUN_TEST(test_shrinking_limits_in_auto_clamps_existing_output);
    RUN_TEST(test_integral_accumulates_per_sample);
    RUN_TEST(test_integral_windup_is_limited);
    RUN_TEST(test_derivative_on_measurement);
    RUN_TEST(test_reverse_direction_inverts_response);
    RUN_TEST(test_switching_direction_while_running);
    RUN_TEST(test_negative_tunings_rejected);
    RUN_TEST(test_sample_time_change_rescales_integral_gain);
    RUN_TEST(test_bumpless_transfer_from_manual);
    RUN_TEST(test_proportional_on_measurement);
    RUN_TEST(test_millis_rollover);
    return UNITY_END();
}
