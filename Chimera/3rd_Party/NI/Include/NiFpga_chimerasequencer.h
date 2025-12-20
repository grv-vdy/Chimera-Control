/*
 * Generated with the FPGA Interface C API Generator 24.3
 * for NI-RIO 24.3 or later.
 */
#ifndef __NiFpga_chimerasequencer_h__
#define __NiFpga_chimerasequencer_h__

#ifndef NiFpga_Version
   #define NiFpga_Version 243
#endif

#include "NiFpga.h"

/**
 * The filename of the FPGA bitfile.
 *
 * This is a #define to allow for string literal concatenation. For example:
 *
 *    static const char* const Bitfile = "C:\\" NiFpga_chimerasequencer_Bitfile;
 */
#define NiFpga_chimerasequencer_Bitfile "C:\\Users\\yb2\\Chimera-Control\\Chimera\\3rd_Party\\NI\\include\\NiFpga_chimerasequencer.lvbitx"

/**
 * The signature of the FPGA bitfile.
 */
static const char* const NiFpga_chimerasequencer_Signature = "EBC7D4AEF42AF08113B54F3F3FE68CF2";

#if NiFpga_Cpp
extern "C"
{
#endif

typedef enum
{
   NiFpga_chimerasequencer_IndicatorBool_finish_stb_o = 0x1800A,
   NiFpga_chimerasequencer_IndicatorBool_gpio_stb_o = 0x1800E,
   NiFpga_chimerasequencer_IndicatorBool_mem_loaded = 0x18016
} NiFpga_chimerasequencer_IndicatorBool;

typedef enum
{
   NiFpga_chimerasequencer_IndicatorI16_index_count = 0x18002
} NiFpga_chimerasequencer_IndicatorI16;

typedef enum
{
   NiFpga_chimerasequencer_IndicatorU64_curr_ts_o = 0x18004,
   NiFpga_chimerasequencer_IndicatorU64_data0 = 0x18018,
   NiFpga_chimerasequencer_IndicatorU64_data1 = 0x1801C,
   NiFpga_chimerasequencer_IndicatorU64_data2 = 0x18020,
   NiFpga_chimerasequencer_IndicatorU64_time0 = 0x18024,
   NiFpga_chimerasequencer_IndicatorU64_time1 = 0x18028,
   NiFpga_chimerasequencer_IndicatorU64_time2 = 0x1802C
} NiFpga_chimerasequencer_IndicatorU64;

typedef enum
{
   NiFpga_chimerasequencer_ControlBool_reset = 0x18012,
   NiFpga_chimerasequencer_ControlBool_skip_program = 0x1803A,
   NiFpga_chimerasequencer_ControlBool_start_copy_to_ram = 0x18036,
   NiFpga_chimerasequencer_ControlBool_trigger_i = 0x18032
} NiFpga_chimerasequencer_ControlBool;

typedef enum
{
   NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Data = 1,
   NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Time = 0
} NiFpga_chimerasequencer_HostToTargetFifoU64;


#if NiFpga_Cpp
}
#endif

#endif
