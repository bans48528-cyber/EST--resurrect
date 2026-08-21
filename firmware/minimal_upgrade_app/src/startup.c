#include <stdint.h>

#include <libopencm3/cm3/scb.h>

#include "platform.h"

typedef void (*init_function_t)(void);

#define ENTRY_SIGNATURE_PHASES 6U

extern uint32_t _data_loadaddr;
extern uint32_t _data;
extern uint32_t _edata;
extern uint32_t _ebss;
extern init_function_t __preinit_array_start;
extern init_function_t __preinit_array_end;
extern init_function_t __init_array_start;
extern init_function_t __init_array_end;

int main(void);
void reset_handler(void) __attribute__((naked, noreturn));

static void reset_handler_c(void) __attribute__((noreturn, used));
static void prepare_board_before_ram_init(void);
static void early_diag_set(uint32_t stage);
static void early_delay(void);
static void early_entry_signature(void);

void reset_handler(void)
{
	__asm volatile(
		"cpsid i\n"
		"bl reset_handler_c\n"
		"b .\n");
}

static void prepare_board_before_ram_init(void)
{
	volatile uint32_t *const rcc_ahb1enr = (uint32_t *)0x40023830U;
	volatile uint32_t *const gpioc_moder = (uint32_t *)0x40020800U;
	volatile uint32_t *const gpioc_bsrr = (uint32_t *)0x40020818U;
	volatile uint32_t *const gpioe_moder = (uint32_t *)0x40021000U;
	volatile uint32_t *const gpioe_bsrr = (uint32_t *)0x40021018U;
	volatile uint32_t *const gpiof_moder = (uint32_t *)0x40021400U;
	volatile uint32_t *const gpiof_bsrr = (uint32_t *)0x40021418U;

	*rcc_ahb1enr |= (1U << 2U) | (1U << 4U) | (1U << 5U);
	(void)*rcc_ahb1enr;

	*gpioe_bsrr = (1U << 2U);
	*gpioe_moder = (*gpioe_moder & ~(3U << (2U * 2U))) |
		(1U << (2U * 2U));

	*gpiof_bsrr = (1U << 2U);
	*gpioc_bsrr = (1U << (13U + 16U));
	*gpiof_moder = (*gpiof_moder & ~(3U << (2U * 2U))) |
		(1U << (2U * 2U));
	*gpioc_moder = (*gpioc_moder & ~(3U << (13U * 2U))) |
		(1U << (13U * 2U));
}

static void early_diag_set(uint32_t stage)
{
	volatile uint32_t *const gpioc_bsrr = (uint32_t *)0x40020818U;
	volatile uint32_t *const gpiof_bsrr = (uint32_t *)0x40021418U;

	if (stage == 0U) {
		*gpiof_bsrr = (1U << (2U + 16U));
		*gpioc_bsrr = (1U << (13U + 16U));
	} else if (stage == 1U) {
		*gpiof_bsrr = (1U << 2U);
		*gpioc_bsrr = (1U << (13U + 16U));
	} else if (stage == 2U) {
		*gpiof_bsrr = (1U << (2U + 16U));
		*gpioc_bsrr = (1U << 13U);
	} else {
		*gpiof_bsrr = (1U << 2U);
		*gpioc_bsrr = (1U << 13U);
	}
}

static void early_delay(void)
{
	volatile uint32_t *const iwdg_kr = (uint32_t *)0x40003000U;
	volatile uint32_t count;
	uint32_t chunk;

	for (chunk = 0U; chunk < 6U; chunk++) {
		*iwdg_kr = 0xAAAAU;
		for (count = 0U; count < 2000000U; count++) {
			__asm volatile("nop");
		}
	}
}

static void early_entry_signature(void)
{
	uint32_t phase;

	for (phase = 0U; phase < ENTRY_SIGNATURE_PHASES; phase++) {
		early_diag_set((phase & 1U) == 0U ? 2U : 1U);
		early_delay();
	}
	early_diag_set(0U);
	early_delay();
}

static void reset_handler_c(void)
{
	uint32_t *source;
	uint32_t *destination;
	init_function_t *function;

	prepare_board_before_ram_init();
	early_entry_signature();
	early_diag_set(1U);

	for (source = &_data_loadaddr, destination = &_data;
	     destination < &_edata; source++, destination++) {
		*destination = *source;
	}
	while (destination < &_ebss) {
		*destination++ = 0U;
	}
	early_diag_set(2U);

	SCB_CCR |= SCB_CCR_STKALIGN;
	platform_prepare_from_bootloader();
	early_diag_set(3U);

	for (function = &__preinit_array_start;
	     function < &__preinit_array_end; function++) {
		(*function)();
	}
	for (function = &__init_array_start;
	     function < &__init_array_end; function++) {
		(*function)();
	}

	(void)main();
	while (1) {
	}
}
