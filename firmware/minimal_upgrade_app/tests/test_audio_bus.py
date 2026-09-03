from pathlib import Path
import unittest

from cffi import FFI


ROOT = Path(__file__).resolve().parents[1]


class AudioBusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ffi = FFI()
        cls.ffi.cdef("""
            bool board_audio_bus_write(uint8_t, uint16_t);
            bool board_audio_bus_send(const uint8_t *, size_t);
            void board_audio_bus_init(void);
            void board_audio_bus_reset(bool);
            uint8_t board_audio_bus_pin_layout(void);
            uint8_t board_audio_bus_pin_probe(void);
            void mock_init(void);
            extern bool mock_dreq, mock_stuck, mock_reset;
            extern unsigned mock_violations, mock_polls, mock_accesses;
            extern unsigned mock_layout, mock_output_pins;
        """)
        stub = r"""
            #include <stdbool.h>
            #include <stdint.h>
            #include <stddef.h>
            #define GPIOA 0
            #define GPIOG 1
            #define GPIO4 16
            #define GPIO5 32
            #define GPIO6 64
            #define GPIO7 128
            #define GPIO15 32768
            #define RCC_GPIOG 1
            #define GPIO_MODE_OUTPUT 0
            #define GPIO_MODE_INPUT 1
            #define GPIO_PUPD_NONE 0
            #define GPIO_PUPD_PULLUP 1
            #define GPIO_PUPD_PULLDOWN 2
            #define AUDIO_PIN_SETTLE_ITERATIONS 4
            #define GPIO_OTYPE_PP 0
            #define GPIO_OSPEED_50MHZ 0
            #define SPI3 3
            #define SPI_SR_TXE 2
            #define SPI_SR_RXNE 1
            #define SPI_SR_BSY 128
            #define SPI_CR1_BAUDRATE_FPCLK_DIV_32 32
            #define SPI_CR1_BAUDRATE_FPCLK_DIV_64 64
            bool mock_dreq, mock_stuck, mock_reset;
            unsigned mock_violations, mock_polls, mock_accesses;
            unsigned mock_layout, mock_output_pins;
            static unsigned pullups;
            static unsigned busy, selected, data;
            void mock_init(void) {
                mock_dreq = true; mock_stuck = mock_reset = false;
                mock_violations = mock_polls = mock_accesses = 0;
                busy = selected = data = 0;
                mock_layout = 1; mock_output_pins = pullups = 0;
            }
            static unsigned status(void) {
                mock_polls++;
                if (busy) {
                    if (!mock_stuck) busy--;
                    return SPI_SR_TXE | SPI_SR_RXNE | SPI_SR_BSY;
                }
                return SPI_SR_TXE | SPI_SR_RXNE;
            }
            static unsigned *data_register(void) {
                busy = 3; mock_accesses++; return &data;
            }
            #define SPI_SR(x) status()
            #define SPI_DR(x) (*data_register())
            static void gpio_set(int port, unsigned pins) {
                if (port == GPIOG) {
                    if (pins & selected) {
                        if (busy && !mock_reset) mock_violations++;
                        selected &= ~pins;
                    }
                    if (pins & (mock_layout == 2 ? GPIO7 : GPIO6)) mock_reset = false;
                }
            }
            static void gpio_clear(int port, unsigned pins) {
                if (port == GPIOG) {
                    selected |= pins & (GPIO4 | (mock_layout == 2 ? GPIO5 : GPIO7));
                    if (pins & (mock_layout == 2 ? GPIO7 : GPIO6)) mock_reset = true;
                }
            }
            static unsigned gpio_get(int port, unsigned pins) {
                unsigned driven = mock_layout == 1 ? GPIO5 :
                    mock_layout == 2 ? GPIO6 : mock_layout == 3 ? (GPIO5 | GPIO6) : 0;
                (void)port;
                return pins & ((mock_dreq ? driven : 0) | (pullups & ~driven));
            }
            static void gpio_mode_setup(int port, int mode, int pull, unsigned pins) {
                unsigned driven = mock_layout == 1 ? GPIO5 :
                    mock_layout == 2 ? GPIO6 : mock_layout == 3 ? (GPIO5 | GPIO6) : 0;
                (void)port;
                if (mode == GPIO_MODE_OUTPUT) {
                    mock_output_pins |= pins;
                    if (pins & driven) mock_violations++;
                } else {
                    mock_output_pins &= ~pins;
                    if (pull == GPIO_PUPD_PULLUP) pullups |= pins;
                    else pullups &= ~pins;
                }
            }
            static void spi_disable(int spi) {
                (void)spi; if (busy && !mock_reset) mock_violations++;
            }
            #define spi_enable(x) ((void)(x))
            #define spi_set_baudrate_prescaler(x,y) ((void)0)
            #define spi_set_clock_polarity_0(x) ((void)0)
            #define spi_set_clock_phase_0(x) ((void)0)
            #define rcc_periph_clock_enable(x) ((void)0)
            #define gpio_set_output_options(a,b,c,d) ((void)0)
        """
        source = (ROOT / "src/board_audio_bus.c").read_text(encoding="utf-8")
        source = "\n".join(line for line in source.splitlines()
                           if not line.startswith("#include <libopencm3/"))
        cls.native = cls.ffi.verify(stub + source, include_dirs=[str(ROOT / "include")])

    def setUp(self):
        self.native.mock_init()
        self.native.board_audio_bus_init()
        self.native.board_audio_bus_reset(False)

    def test_register_and_stream_wait_for_last_clock_before_deselect(self):
        n = self.native
        self.assertTrue(n.board_audio_bus_write(3, 0x9800))
        self.assertTrue(n.board_audio_bus_send(bytes(range(32)), 32))
        self.assertEqual(n.mock_violations, 0)
        self.assertEqual(n.mock_accesses, (4 + 32) * 2)

    def test_stuck_busy_is_bounded_and_resets_decoder(self):
        n = self.native
        n.mock_stuck = True
        self.assertFalse(n.board_audio_bus_send(b"x", 1))
        self.assertTrue(n.mock_reset)
        self.assertEqual(n.mock_violations, 0)
        self.assertLess(n.mock_polls, 10100)

    def test_no_transfer_without_dreq_or_with_oversized_chunk(self):
        n = self.native
        n.mock_dreq = False
        self.assertFalse(n.board_audio_bus_write(3, 0x9800))
        self.assertFalse(n.board_audio_bus_send(b"x", 1))
        n.mock_dreq = True
        self.assertFalse(n.board_audio_bus_send(bytes(33), 33))
        self.assertEqual(n.mock_accesses, 0)

    def test_both_known_layouts_keep_the_real_dreq_as_input(self):
        n = self.native
        for layout, probe, dreq, reset in ((1, 7, 32, 64), (2, 13, 64, 128)):
            n.mock_init()
            n.mock_layout = layout
            n.board_audio_bus_init()
            self.assertEqual(n.board_audio_bus_pin_layout(), layout)
            self.assertEqual(n.board_audio_bus_pin_probe(), probe)
            self.assertEqual(n.mock_output_pins & dreq, 0)
            self.assertEqual(n.mock_output_pins & reset, reset)
            self.assertTrue(n.mock_reset)
            n.board_audio_bus_reset(False)
            self.assertTrue(n.board_audio_bus_write(3, 0x9800))
            self.assertTrue(n.board_audio_bus_send(b"mp3", 3))
            self.assertEqual(n.mock_violations, 0)

    def test_ambiguous_or_missing_decoder_never_enables_audio_outputs(self):
        n = self.native
        for layout in (0, 3):
            n.mock_init()
            n.mock_layout = layout
            n.board_audio_bus_init()
            self.assertEqual(n.board_audio_bus_pin_layout(), 0)
            self.assertEqual(n.mock_output_pins & (32 | 64 | 128), 0)
            self.assertFalse(n.board_audio_bus_write(3, 0x9800))
            self.assertFalse(n.board_audio_bus_send(b"mp3", 3))
            self.assertEqual(n.mock_accesses, 0)
            self.assertEqual(n.mock_violations, 0)


if __name__ == "__main__":
    unittest.main()
