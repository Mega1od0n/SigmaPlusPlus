#pragma once
#include <cstdint>

#include "VM.h"

void runtime_print(int64_t v);
void runtime_print_f_bits(int64_t bits);

int64_t runtime_array_new(VM* vm, int64_t size);
int64_t runtime_array_get(VM* vm, int64_t arr_id, int64_t idx);
void runtime_array_set(VM* vm, int64_t arr_id, int64_t idx, int64_t val);
int64_t runtime_array_len(VM* vm, int64_t arr_id);

int64_t runtime_call_function(VM* vm, uint32_t func_id, int64_t* args, uint32_t argc);

int64_t runtime_time_ms();
int64_t runtime_rand();

int64_t runtime_sqrt_bits(int64_t x_bits);
void runtime_print_big(VM* vm, int64_t handle, int64_t len);
