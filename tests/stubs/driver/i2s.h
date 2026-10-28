#pragma once
#include <Arduino.h>
#include <esp_err.h>
enum i2s_mode_t { I2S_MODE_MASTER=1, I2S_MODE_RX=2 };
enum i2s_bits_per_sample_t { I2S_BITS_PER_SAMPLE_32BIT=32 };
enum i2s_channel_fmt_t { I2S_CHANNEL_FMT_RIGHT_LEFT, I2S_CHANNEL_FMT_ONLY_RIGHT };
enum i2s_comm_format_t { I2S_COMM_FORMAT_STAND_MSB, I2S_COMM_FORMAT_STAND_I2S };
enum i2s_mclk_multiple_t { I2S_MCLK_MULTIPLE_256=256 };
enum i2s_bits_per_chan_t { I2S_BITS_PER_CHAN_32BIT=32 };
struct i2s_config_t { i2s_mode_t mode; int sample_rate; i2s_bits_per_sample_t bits_per_sample; i2s_channel_fmt_t channel_format; i2s_comm_format_t communication_format; int intr_alloc_flags,dma_buf_count,dma_buf_len; bool use_apll,tx_desc_auto_clear; int fixed_mclk; i2s_mclk_multiple_t mclk_multiple; i2s_bits_per_chan_t bits_per_chan; };
struct i2s_pin_config_t { int mck_io_num,bck_io_num,ws_io_num,data_out_num,data_in_num; };
#define I2S_NUM_0 0
#define I2S_PIN_NO_CHANGE -1
esp_err_t i2s_driver_uninstall(int); esp_err_t i2s_driver_install(int,const i2s_config_t*,int,void*); esp_err_t i2s_set_pin(int,const i2s_pin_config_t*); esp_err_t i2s_read(int,void*,size_t,size_t*,int);
