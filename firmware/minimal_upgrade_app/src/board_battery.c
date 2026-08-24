#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "board_battery.h"

#define BATTERY_ADC_PORT GPIOF
#define BATTERY_ADC_PIN GPIO3
#define BATTERY_ADC ADC3
#define BATTERY_ADC_CHANNEL ADC_CHANNEL9

#define BATTERY_SAMPLE_INTERVAL_MS 100U
#define BATTERY_FILTER_SAMPLES 8U
#define BATTERY_SCALE_MV 2500U
#define BATTERY_ADC_FULL_SCALE 4096U

/* Preserve the rechargeable-pack thresholds used by the original EST APP. */
#define BATTERY_LEVEL_4_MIN_MV 1661U
#define BATTERY_LEVEL_3_MIN_MV 1591U
#define BATTERY_LEVEL_2_MIN_MV 1521U
#define BATTERY_LEVEL_1_MIN_MV 1451U

static uint16_t samples[BATTERY_FILTER_SAMPLES];
static uint32_t sample_sum;
static uint32_t last_sample_ms;
static uint8_t sample_count;
static uint8_t sample_index;
static struct board_battery_snapshot current_snapshot;

static uint16_t read_adc_sample(void)
{
	uint8_t sequence[1] = {BATTERY_ADC_CHANNEL};

	adc_set_regular_sequence(BATTERY_ADC, 1U, sequence);
	adc_start_conversion_regular(BATTERY_ADC);
	while (!adc_eoc(BATTERY_ADC)) {
		/* A 144-cycle conversion completes in far less than one millisecond. */
	}
	return (uint16_t)adc_read_regular(BATTERY_ADC);
}

static uint8_t level_from_sample_mv(uint16_t sample_mv)
{
	if (sample_mv >= BATTERY_LEVEL_4_MIN_MV) {
		return 4U;
	}
	if (sample_mv >= BATTERY_LEVEL_3_MIN_MV) {
		return 3U;
	}
	if (sample_mv >= BATTERY_LEVEL_2_MIN_MV) {
		return 2U;
	}
	if (sample_mv >= BATTERY_LEVEL_1_MIN_MV) {
		return 1U;
	}
	return 0U;
}

static void add_sample(uint16_t sample)
{
	uint16_t average;

	if (sample_count == BATTERY_FILTER_SAMPLES) {
		sample_sum -= samples[sample_index];
	} else {
		sample_count++;
	}
	samples[sample_index] = sample;
	sample_sum += sample;
	sample_index = (uint8_t)((sample_index + 1U) % BATTERY_FILTER_SAMPLES);
	average = (uint16_t)(sample_sum / sample_count);
	current_snapshot.valid = true;
	current_snapshot.adc_raw = average;
	current_snapshot.sample_mv = (uint16_t)(((uint32_t)average *
		BATTERY_SCALE_MV) / BATTERY_ADC_FULL_SCALE);
	current_snapshot.level = level_from_sample_mv(current_snapshot.sample_mv);
}

void board_battery_init(uint32_t now_ms)
{
	uint8_t index;

	rcc_periph_clock_enable(RCC_GPIOF);
	rcc_periph_clock_enable(RCC_ADC3);
	gpio_mode_setup(BATTERY_ADC_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
		BATTERY_ADC_PIN);
	adc_power_off(BATTERY_ADC);
	adc_disable_scan_mode(BATTERY_ADC);
	adc_set_single_conversion_mode(BATTERY_ADC);
	adc_set_right_aligned(BATTERY_ADC);
	adc_set_resolution(BATTERY_ADC, ADC_CR1_RES_12BIT);
	adc_disable_external_trigger_regular(BATTERY_ADC);
	adc_set_sample_time(BATTERY_ADC, BATTERY_ADC_CHANNEL, ADC_SMPR_SMP_144CYC);
	adc_power_on(BATTERY_ADC);

	memset(samples, 0, sizeof(samples));
	memset(&current_snapshot, 0, sizeof(current_snapshot));
	sample_sum = 0U;
	sample_count = 0U;
	sample_index = 0U;
	for (index = 0U; index < BATTERY_FILTER_SAMPLES; index++) {
		add_sample(read_adc_sample());
	}
	last_sample_ms = now_ms;
}

void board_battery_tick(uint32_t now_ms)
{
	if ((uint32_t)(now_ms - last_sample_ms) < BATTERY_SAMPLE_INTERVAL_MS) {
		return;
	}
	last_sample_ms = now_ms;
	add_sample(read_adc_sample());
}

struct board_battery_snapshot board_battery_snapshot(void)
{
	return current_snapshot;
}
