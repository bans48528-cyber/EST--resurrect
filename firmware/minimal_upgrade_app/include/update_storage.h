#ifndef UPDATE_STORAGE_H
#define UPDATE_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool update_storage_begin(void);
bool update_storage_write(uint32_t offset, const uint8_t *data, size_t length);
bool update_storage_validate_image(uint32_t package_length);
bool update_storage_commit(uint32_t package_length);
void update_storage_abort(void);

#endif
