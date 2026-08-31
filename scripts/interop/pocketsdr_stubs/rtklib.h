// Stub rtklib.h — PocketSDR's pocket_sdr.h references several RTKLIB types
// in struct field declarations (gtime_t, obs_t, nav_t, sol_t, ssat_t,
// rtcm_t, stream_t). We never call any function taking or returning these
// types from the code-generation path, but the compiler still needs to see
// the names. Forward-declared opaque structs satisfy that without cloning
// RTKLIB.

#pragma once

#include <stdint.h>
#include <time.h>

typedef struct { time_t time; double sec; } gtime_t;

typedef struct obs_t    obs_t;
typedef struct nav_t    nav_t;
typedef struct sol_t    sol_t;
typedef struct ssat_t   ssat_t;
typedef struct rtcm_t   rtcm_t;
typedef struct stream_t stream_t;

// libusb forward decls — pocket_sdr.h references these in a struct for
// the USB device handle, but sdr_code.c never touches USB.
typedef struct libusb_context       libusb_context;
typedef struct libusb_device_handle libusb_device_handle;
