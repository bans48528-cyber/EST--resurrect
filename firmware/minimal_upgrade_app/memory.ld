MEMORY
{
	rom (rx)  : ORIGIN = 0x08010000, LENGTH = 0x70000
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 0x2D3B0
}

INCLUDE cortex-m-generic.ld

ASSERT(ORIGIN(rom) == 0x08010000, "APP vector address changed")
ASSERT(ORIGIN(rom) + LENGTH(rom) == 0x08080000, "APP/Update boundary changed")
