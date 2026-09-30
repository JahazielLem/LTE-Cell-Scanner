// SoapySDR front-end support for LTE-Cell-Scanner (HackRF, PlutoSDR, ...).
//
// Adds a generic SoapySDR capture backend so the scanner can run on SDRs that
// do not have a native RTL-SDR interface. The scanner works at 1.92 Msps
// (FS_LTE/16); since many SDRs (e.g. HackRF) cannot sample that low, samples are
// captured at an integer multiple of 1.92 Msps and decimated with an
// anti-aliasing FIR down to 1.92 Msps.
//
// This file is released under the same AGPLv3 license as the rest of the project.

#ifndef HAVE_RTL_SOAPY_H
#define HAVE_RTL_SOAPY_H

#ifdef HAVE_SOAPYSDR

#include <string>
#include <vector>
#include <complex>
#include <itpp/itbase.h>

namespace SoapySDR { class Device; class Stream; }

// Handle bundling a SoapySDR device, its RX stream and the decimation setup.
struct soapy_dev_t {
  SoapySDR::Device * dev;
  SoapySDR::Stream * stream;
  int                oversample;   // capture_rate = oversample * 1.92e6
  double             fs_capture;   // actual capture sample rate (Hz)
  std::vector<float> fir;          // anti-alias FIR (real taps)
  std::vector<std::complex<float> > hist; // filter history between reads

  soapy_dev_t() : dev(0), stream(0), oversample(0), fs_capture(0) {}
};

// Open and configure a SoapySDR device.
//   args      : SoapySDR device args, e.g. "driver=hackrf" or "driver=plutosdr"
//   fc        : initial center frequency (Hz)
//   gain_db   : overall RX gain in dB (<0 selects a sensible default)
// Returns the programmed sample rate (1.92e6) in fs_programmed.
void soapy_config(
  const std::string & args,
  const double & fc,
  const double & gain_db,
  soapy_dev_t & s,
  double & fs_programmed
);

// Retune and capture `n_out` decimated samples (at 1.92 Msps) into capbuf.
void soapy_capture_data(
  soapy_dev_t & s,
  const double & fc_requested,
  itpp::cvec & capbuf,
  double & fc_programmed
);

// Read exactly n_out decimated samples (1.92 Msps) as raw interleaved unsigned
// 8-bit IQ (compatible with the RTL-SDR fifo used by LTE-Tracker).
// Returns the number of decimated IQ sample pairs written.
int soapy_read_u8(
  soapy_dev_t & s,
  unsigned char * out,
  int n_out
);

// Set the center frequency and report what was actually programmed.
void soapy_set_freq(soapy_dev_t & s, const double & fc, double & fc_programmed);

void soapy_close(soapy_dev_t & s);

#endif // HAVE_SOAPYSDR
#endif // HAVE_RTL_SOAPY_H
