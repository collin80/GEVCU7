/*
 * Throttle base class: pedal position -> signed torque request mapping
 * (regen ramp, coast gap, 3-point acceleration curve, creep), the "smart
 * smoothing" moving average, and config persistence. A minimal subclass
 * injects the pedal position so the real Throttle code is what is tested.
 */
#include "gevcu_test.h"
#include "devices/io/Throttle.h"
#include "DeviceManager.h"

class FakeThrottle : public Throttle
{
public:
    FakeThrottle()
    {
        deviceId = THROTTLE;
        setConfiguration(&cfg);
    }
    int16_t mapPedal(int16_t p) { return Throttle::mapPedalPosition(p); }
    using Throttle::normalizeInput;
    using Throttle::normalizeAndConstrainInput;

    ThrottleConfiguration cfg;
    int16_t pedal = 0;
    bool valid = true;
    bool identityMap = false;

protected:
    bool validateSignal(RawSignalData *) override { return valid; }
    int16_t calculatePedalPosition(RawSignalData *) override { return pedal; }
    int16_t mapPedalPosition(int16_t p) override { return identityMap ? p : Throttle::mapPedalPosition(p); }
};

static FakeThrottle *t;

static void defaultConfig(ThrottleConfiguration &c)
{
    c.positionRegenMaximum = 0;
    c.positionRegenMinimum = 270;
    c.positionForwardMotionStart = 280;
    for (auto &mp : c.mapPoints) { mp.inputPosition = 750; mp.outputPosition = 500; }
    c.minimumRegen = 0;
    c.maximumRegen = 70;
    c.creep = 0;
    c.smartSmooth = 0;
    c.smoothStop = 600;
}

static void threePointCurve(ThrottleConfiguration &c)
{
    c.mapPoints[0] = {400, 200};
    c.mapPoints[1] = {600, 500};
    c.mapPoints[2] = {800, 800};
}

static int16_t tickWith(int16_t pedal)
{
    t->pedal = pedal;
    t->handleTick();
    return t->getLevel();
}

void setUp()
{
    hostResetAll();
    memCache = new MemCache();
    memCache->setup();
    t = new FakeThrottle();
    defaultConfig(t->cfg);
    t->setup();
}

void tearDown()
{
    delete t;
    delete memCache;
    memCache = nullptr;
}

// ------------------------------------------------------------- input math

void test_normalize_input()
{
    TEST_ASSERT_EQUAL_INT32(0, t->normalizeInput(100, 100, 3100));
    TEST_ASSERT_EQUAL_INT32(500, t->normalizeInput(1600, 100, 3100));
    TEST_ASSERT_EQUAL_INT32(1000, t->normalizeInput(3100, 100, 3100));
    TEST_ASSERT_EQUAL_INT32(-100, t->normalizeInput(-200, 100, 3100)); // unconstrained
}

void test_normalize_inverted_pot()
{
    // Pots wired backwards have min > max; mapping must still go 0..1000.
    TEST_ASSERT_EQUAL_INT32(0, t->normalizeInput(3000, 3000, 1000));
    TEST_ASSERT_EQUAL_INT32(1000, t->normalizeInput(1000, 3000, 1000));
}

void test_normalize_and_constrain()
{
    TEST_ASSERT_EQUAL_INT16(0, t->normalizeAndConstrainInput(0, 100, 3100));
    TEST_ASSERT_EQUAL_INT16(1000, t->normalizeAndConstrainInput(4000, 100, 3100));
    TEST_ASSERT_EQUAL_INT16(250, t->normalizeAndConstrainInput(850, 100, 3100));
}

// --------------------------------------------------------------- mapping

void test_regen_ramp()
{
    TEST_ASSERT_EQUAL_INT16(-700, t->mapPedal(0));   // full max regen with foot off
    TEST_ASSERT_EQUAL_INT16(-350, t->mapPedal(135)); // half way down the ramp
    TEST_ASSERT_EQUAL_INT16(0, t->mapPedal(270));    // regen min point
}

void test_regen_ramp_with_minimum_regen()
{
    t->cfg.minimumRegen = 10;
    TEST_ASSERT_EQUAL_INT16(-700, t->mapPedal(0));
    TEST_ASSERT_EQUAL_INT16(-100, t->mapPedal(270));
}

void test_coast_gap_is_zero()
{
    for (int p = 271; p < 280; p++) TEST_ASSERT_EQUAL_INT16(0, t->mapPedal(p));
}

void test_below_regen_maximum_is_zero()
{
    t->cfg.positionRegenMaximum = 100;
    TEST_ASSERT_EQUAL_INT16(0, t->mapPedal(50));
    TEST_ASSERT_EQUAL_INT16(-700, t->mapPedal(100));
}

void test_regen_disabled_when_min_equals_max()
{
    t->cfg.positionRegenMaximum = 0;
    t->cfg.positionRegenMinimum = 0;
    TEST_ASSERT_EQUAL_INT16(0, t->mapPedal(0)); // no divide by zero
}

void test_forward_default_curve()
{
    TEST_ASSERT_EQUAL_INT16(0, t->mapPedal(280));
    TEST_ASSERT_EQUAL_INT16(250, t->mapPedal(515));
    TEST_ASSERT_EQUAL_INT16(500, t->mapPedal(750));
    TEST_ASSERT_EQUAL_INT16(750, t->mapPedal(875));
    TEST_ASSERT_EQUAL_INT16(1000, t->mapPedal(1000));
}

void test_top_end_snaps_to_full_power()
{
    TEST_ASSERT_EQUAL_INT16(978, t->mapPedal(989));
    TEST_ASSERT_EQUAL_INT16(1000, t->mapPedal(990));
}

void test_three_point_curve()
{
    threePointCurve(t->cfg);
    TEST_ASSERT_EQUAL_INT16(100, t->mapPedal(340));
    TEST_ASSERT_EQUAL_INT16(200, t->mapPedal(400));
    TEST_ASSERT_EQUAL_INT16(350, t->mapPedal(500));
    TEST_ASSERT_EQUAL_INT16(500, t->mapPedal(600));
    TEST_ASSERT_EQUAL_INT16(650, t->mapPedal(700));
    TEST_ASSERT_EQUAL_INT16(800, t->mapPedal(800));
    TEST_ASSERT_EQUAL_INT16(900, t->mapPedal(900));
}

void test_curve_is_monotonic_and_bounded()
{
    threePointCurve(t->cfg);
    int16_t prev = t->mapPedal(0);
    for (int p = 1; p <= 1000; p++)
    {
        int16_t v = t->mapPedal(p);
        TEST_ASSERT_TRUE_MESSAGE(v >= prev, "throttle output decreased while pedal increased");
        TEST_ASSERT_TRUE(v >= -1000 && v <= 1000);
        prev = v;
    }
    TEST_ASSERT_EQUAL(Throttle::OK, t->getStatus());
}

void test_creep_at_zero_pedal()
{
    t->cfg.creep = 5;
    TEST_ASSERT_EQUAL_INT16(50, t->mapPedal(0));
    // Creep only applies with the pedal fully released; any travel is back in the regen ramp.
    TEST_ASSERT_EQUAL_INT16(-700, t->mapPedal(1));
}

void test_out_of_range_curve_raises_fault()
{
    t->cfg.mapPoints[0] = {400, 200};
    t->cfg.mapPoints[1] = {600, 500};
    t->cfg.mapPoints[2] = {800, 1200}; // misconfigured: above 100%
    TEST_ASSERT_EQUAL_INT16(0, t->mapPedal(900));
    TEST_ASSERT_EQUAL(Throttle::ERR_MISC, t->getStatus());
    TEST_ASSERT_TRUE(t->isFaulted());
    TEST_ASSERT_TRUE(hostLogContains(4, "Fault 3e8 raised by device 1030"));
}

// --------------------------------------------------------- tick pipeline

void test_invalid_signal_zeroes_output()
{
    TEST_ASSERT_EQUAL_INT16(500, tickWith(750));
    t->valid = false;
    TEST_ASSERT_EQUAL_INT16(0, tickWith(750));
}

void test_no_smoothing_passes_through()
{
    t->identityMap = true;
    TEST_ASSERT_EQUAL_INT16(100, tickWith(100));
    TEST_ASSERT_EQUAL_INT16(400, tickWith(400));
}

void test_smart_smoothing_average()
{
    t->identityMap = true;
    t->cfg.smartSmooth = 4;
    t->cfg.smoothStop = 600;
    TEST_ASSERT_EQUAL_INT16(1000, tickWith(1000)); // above cutoff: direct
    // Dropping below the cutoff seeds the buffer with the cutoff value, then averages in.
    TEST_ASSERT_EQUAL_INT16(500, tickWith(200));
    TEST_ASSERT_EQUAL_INT16(400, tickWith(200));
    TEST_ASSERT_EQUAL_INT16(300, tickWith(200));
    TEST_ASSERT_EQUAL_INT16(200, tickWith(200));
    TEST_ASSERT_EQUAL_INT16(200, tickWith(200)); // wraps around the buffer
}

void test_smart_smoothing_released_above_cutoff()
{
    t->identityMap = true;
    t->cfg.smartSmooth = 8;
    t->cfg.smoothStop = 600;
    tickWith(1000);
    tickWith(100);
    TEST_ASSERT_EQUAL_INT16(700, tickWith(700)); // at/above cutoff: no lag
}

void test_smart_smoothing_converges()
{
    t->identityMap = true;
    t->cfg.smartSmooth = 100;
    t->cfg.smoothStop = 1000;
    for (int i = 0; i < 100; i++) tickWith(300);
    TEST_ASSERT_EQUAL_INT16(300, t->getLevel());
}

// ---------------------------------------------------- config and registry

void test_setup_registers_config_entries()
{
    const char *names[] = {"TRNGMAX", "TRNGMIN", "TFWD", "TMAP1IN", "TMAP1OUT", "TMAP2IN", "TMAP2OUT",
                           "TMAP3IN", "TMAP3OUT", "TMINRN", "TMAXRN", "TCREEP", "TSMOOTH", "TSMOOTHCUT"};
    TEST_ASSERT_EQUAL_UINT(14, t->getConfigEntries()->size());
    for (const char *n : names) TEST_ASSERT_NOT_NULL_MESSAGE(t->findConfigEntry(n), n);
    const ConfigEntry *e = t->findConfigEntry("TMAP2OUT");
    TEST_ASSERT_EQUAL_PTR(&t->cfg.mapPoints[1].outputPosition, e->varPtr);
    TEST_ASSERT_EQUAL(CFG_ENTRY_VAR_TYPE::UINT16, e->varType);
    TEST_ASSERT_EQUAL_INT8(-10, e->precision); // stored in 0.1% units, shown as %
    TEST_ASSERT_NULL(t->findConfigEntry("tmap2out")); // console names are exact
}

void test_setup_registers_status_entries()
{
    bool level = false, pedal = false, raw = false;
    for (auto &s : hostStatusEntries())
    {
        if (s.statusName == "Throttle_Level") level = true;
        if (s.statusName == "Pedal_Position") pedal = true;
        if (s.statusName == "Raw_Throttle") raw = true;
    }
    TEST_ASSERT_TRUE(level && pedal && raw);
}

void test_load_configuration_defaults()
{
    t->earlyInit();
    memset(&t->cfg, 0xAB, sizeof(t->cfg));
    t->loadConfiguration();
    TEST_ASSERT_EQUAL_UINT16(270, t->cfg.positionRegenMinimum);
    TEST_ASSERT_EQUAL_UINT16(0, t->cfg.positionRegenMaximum);
    TEST_ASSERT_EQUAL_UINT16(280, t->cfg.positionForwardMotionStart);
    for (auto &mp : t->cfg.mapPoints)
    {
        TEST_ASSERT_EQUAL_UINT16(750, mp.inputPosition);
        TEST_ASSERT_EQUAL_UINT16(500, mp.outputPosition);
    }
    TEST_ASSERT_EQUAL_UINT8(0, t->cfg.creep);
    TEST_ASSERT_EQUAL_UINT8(0, t->cfg.minimumRegen);
    TEST_ASSERT_EQUAL_UINT8(70, t->cfg.maximumRegen);
    TEST_ASSERT_EQUAL_UINT8(0, t->cfg.smartSmooth);
    TEST_ASSERT_EQUAL_UINT16(600, t->cfg.smoothStop);
}

void test_save_and_reload_configuration()
{
    t->earlyInit();
    threePointCurve(t->cfg);
    t->cfg.positionRegenMinimum = 200;
    t->cfg.creep = 3;
    t->cfg.maximumRegen = 45;
    t->cfg.smartSmooth = 12;
    t->cfg.smoothStop = 444;
    t->saveConfiguration();
    ThrottleConfiguration saved = t->cfg;

    // Power cycle: new cache, new device instance.
    delete t;
    delete memCache;
    memCache = new MemCache();
    memCache->setup();
    t = new FakeThrottle();
    t->earlyInit();
    t->loadConfiguration();
    TEST_ASSERT_EQUAL_MEMORY(&saved, &t->cfg, sizeof(saved));
}

void test_fault_descriptions()
{
    TEST_ASSERT_EQUAL_STRING("Throttle input 1 is too high", t->getFaultDescription(THROTTLE_FAULT_IN1_TOOHIGH));
    TEST_ASSERT_EQUAL_STRING("Throttle inputs do not agree on position", t->getFaultDescription(THROTTLE_FAULT_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("No fault", t->getFaultDescription(0)); // falls back to generic device faults
    TEST_ASSERT_NULL(t->getFaultDescription(THROTTLE_LAST_FAULT));
    for (int c = THROTTLE_FAULT_IN1_TOOHIGH; c < THROTTLE_LAST_FAULT; c++) TEST_ASSERT_NOT_NULL(t->getFaultDescription(c));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_normalize_input);
    RUN_TEST(test_normalize_inverted_pot);
    RUN_TEST(test_normalize_and_constrain);
    RUN_TEST(test_regen_ramp);
    RUN_TEST(test_regen_ramp_with_minimum_regen);
    RUN_TEST(test_coast_gap_is_zero);
    RUN_TEST(test_below_regen_maximum_is_zero);
    RUN_TEST(test_regen_disabled_when_min_equals_max);
    RUN_TEST(test_forward_default_curve);
    RUN_TEST(test_top_end_snaps_to_full_power);
    RUN_TEST(test_three_point_curve);
    RUN_TEST(test_curve_is_monotonic_and_bounded);
    RUN_TEST(test_creep_at_zero_pedal);
    RUN_TEST(test_out_of_range_curve_raises_fault);
    RUN_TEST(test_invalid_signal_zeroes_output);
    RUN_TEST(test_no_smoothing_passes_through);
    RUN_TEST(test_smart_smoothing_average);
    RUN_TEST(test_smart_smoothing_released_above_cutoff);
    RUN_TEST(test_smart_smoothing_converges);
    RUN_TEST(test_setup_registers_config_entries);
    RUN_TEST(test_setup_registers_status_entries);
    RUN_TEST(test_load_configuration_defaults);
    RUN_TEST(test_save_and_reload_configuration);
    RUN_TEST(test_fault_descriptions);
    return UNITY_END();
}
