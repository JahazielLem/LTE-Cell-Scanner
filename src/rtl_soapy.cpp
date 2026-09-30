// SoapySDR front-end support for LTE-Cell-Scanner (HackRF, PlutoSDR, ...).
// Released under the AGPLv3 license, like the rest of the project.

#ifdef HAVE_SOAPYSDR

#include <iostream>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <itpp/itbase.h>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Errors.hpp>
#include "rtl_soapy.h"
#include "common.h"

using namespace std;

// Target sample rate the scanner works at (FS_LTE/16).
static const double LTE_RATE = 1920000.0;

// ---------------------------------------------------------------------------
// Anti-aliasing FIR design (windowed-sinc low-pass, Hamming window).
// Normalised cutoff is expressed in cycles/sample of the *capture* rate.
// ---------------------------------------------------------------------------
static void design_fir(vector<float> & h, int oversample)
{
  // Cut off at half of the decimated Nyquist -> 0.5/oversample cycles/sample.
  const double fc = 0.5 / (double)oversample;
  const int ntaps = 16 * oversample + 1;   // odd length, linear phase
  const int M = ntaps - 1;
  h.assign(ntaps, 0.0f);
  double sum = 0.0;
  for (int n = 0; n < ntaps; n++) {
    const double m = n - M / 2.0;
    double sinc;
    if (fabs(m) < 1e-9)
      sinc = 2.0 * fc;
    else
      sinc = sin(2.0 * M_PI * fc * m) / (M_PI * m);
    const double w = 0.54 - 0.46 * cos(2.0 * M_PI * n / (double)M); // Hamming
    const double v = sinc * w;
    h[n] = (float)v;
    sum += v;
  }
  // Normalise for unity DC gain.
  for (int n = 0; n < ntaps; n++)
    h[n] = (float)(h[n] / sum);
}

// Read exactly n_in complex-float samples from the RX stream (blocking).
static void soapy_read_input(soapy_dev_t & s, complex<float> * buf, int n_in)
{
  int got = 0;
  while (got < n_in) {
    void * bufs[1] = { (void *)(buf + got) };
    int flags = 0;
    long long timeNs = 0;
    int ret = s.dev->readStream(s.stream, bufs, n_in - got, flags, timeNs, 500000);
    if (ret > 0) {
      got += ret;
    } else if (ret == SOAPY_SDR_OVERFLOW) {
      // Dropped samples upstream; keep going.
      continue;
    } else if (ret == SOAPY_SDR_TIMEOUT) {
      continue;
    } else {
      cerr << "Error: SoapySDR readStream failed: " << SoapySDR::errToStr(ret) << endl;
      throw runtime_error("SoapySDR readStream failed");
    }
  }
}

// Filter + decimate `n_out*oversample` input samples into n_out output samples.
// `s.hist` (length ntaps-1) carries filter state between successive blocks so
// streaming (LTE-Tracker) stays continuous.
static void soapy_decimate(soapy_dev_t & s, int n_out, complex<double> * out)
{
  const int D = s.oversample;
  const int ntaps = (int)s.fir.size();
  const int hlen = ntaps - 1;
  const int n_in = n_out * D;

  // Assemble [history | new input] so we can filter with full context.
  vector<complex<float> > x(hlen + n_in);
  for (int i = 0; i < hlen; i++)
    x[i] = s.hist[i];
  soapy_read_input(s, &x[hlen], n_in);

  // Decimating FIR: output sample k uses input centre index (hlen + k*D).
  for (int k = 0; k < n_out; k++) {
    const int centre = hlen + k * D;   // aligned so taps index back over history
    float acc_r = 0.0f, acc_i = 0.0f;
    for (int t = 0; t < ntaps; t++) {
      const complex<float> & xv = x[centre - t];
      acc_r += s.fir[t] * xv.real();
      acc_i += s.fir[t] * xv.imag();
    }
    out[k] = complex<double>(acc_r, acc_i);
  }

  // Save the last hlen input samples as history for the next block.
  for (int i = 0; i < hlen; i++)
    s.hist[i] = x[n_in + i];
}

void soapy_config(
  const string & args,
  const double & fc,
  const double & gain_db,
  soapy_dev_t & s,
  double & fs_programmed
) {
  s.dev = SoapySDR::Device::make(args);
  if (s.dev == 0) {
    cerr << "Error: unable to open SoapySDR device with args '" << args << "'" << endl;
    throw runtime_error("SoapySDR::Device::make failed");
  }

  // Pick the smallest integer oversample factor that keeps the capture rate at
  // or above 2 Msps (the practical floor for e.g. HackRF).
  s.oversample = 2;
  while (s.oversample * LTE_RATE < 2.0e6)
    s.oversample++;
  s.fs_capture = s.oversample * LTE_RATE;

  s.dev->setSampleRate(SOAPY_SDR_RX, 0, s.fs_capture);
  const double actual_rate = s.dev->getSampleRate(SOAPY_SDR_RX, 0);
  if (fabs(actual_rate - s.fs_capture) > 1.0) {
    // Device rounded the rate; keep it and recompute (decimation still targets
    // an integer factor, small residual error is absorbed by the scanner).
    s.fs_capture = actual_rate;
  }

  // Analog filter bandwidth: keep the full LTE signal, reject far out-of-band.
  try {
    s.dev->setBandwidth(SOAPY_SDR_RX, 0, 1.75e6);
  } catch (...) { /* not all drivers support this */ }

  // Gain. Some drivers (notably SoapyHackRF) do not honour the aggregate
  // setGain() well, so set each named gain element explicitly. A negative
  // gain_db selects a sensible default (or AGC where supported).
  double want = (gain_db >= 0) ? gain_db : 62.0; // default total (dB)
  try { s.dev->setGainMode(SOAPY_SDR_RX, 0, false); } catch (...) {}
  const std::vector<std::string> gnames = s.dev->listGains(SOAPY_SDR_RX, 0);
  if (!gnames.empty()) {
    // Distribute the requested total across the individual elements, filling
    // each up to its range in listed order (e.g. HackRF: LNA, then VGA; AMP off).
    double remaining = want;
    for (size_t i = 0; i < gnames.size(); i++) {
      const SoapySDR::Range r = s.dev->getGainRange(SOAPY_SDR_RX, 0, gnames[i]);
      // Skip the wideband AMP (a coarse 0/14 dB boost) unless a lot is asked for.
      double g;
      if (gnames[i] == "AMP") {
        g = (want >= 60.0) ? r.maximum() : r.minimum();
      } else {
        g = r.minimum() + remaining;
        if (g > r.maximum()) g = r.maximum();
        if (g < r.minimum()) g = r.minimum();
      }
      s.dev->setGain(SOAPY_SDR_RX, 0, gnames[i], g);
      if (gnames[i] != "AMP") remaining -= (g - r.minimum());
      if (remaining < 0) remaining = 0;
    }
  } else {
    s.dev->setGain(SOAPY_SDR_RX, 0, want);
  }

  s.dev->setFrequency(SOAPY_SDR_RX, 0, fc);

  design_fir(s.fir, s.oversample);
  s.hist.assign(s.fir.size() - 1, complex<float>(0.0f, 0.0f));

  s.stream = s.dev->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
  s.dev->activateStream(s.stream);

  fs_programmed = LTE_RATE;

  cout << "SoapySDR: device open, capture rate " << s.fs_capture / 1e6
       << " MHz, decimate x" << s.oversample << " -> " << LTE_RATE / 1e6
       << " MHz" << endl;
}

void soapy_set_freq(soapy_dev_t & s, const double & fc, double & fc_programmed)
{
  s.dev->setFrequency(SOAPY_SDR_RX, 0, fc);
  fc_programmed = s.dev->getFrequency(SOAPY_SDR_RX, 0);
  // Reset filter history and let the front-end settle after retuning.
  fill(s.hist.begin(), s.hist.end(), complex<float>(0.0f, 0.0f));
}

void soapy_capture_data(
  soapy_dev_t & s,
  const double & fc_requested,
  itpp::cvec & capbuf,
  double & fc_programmed
) {
  soapy_set_freq(s, fc_requested, fc_programmed);

  // Discard ~50 ms so the front-end/AGC settles after the retune.
  {
    const int settle = (int)(0.05 * s.fs_capture);
    vector<complex<float> > junk(settle > 0 ? settle : 1);
    soapy_read_input(s, &junk[0], (int)junk.size());
    fill(s.hist.begin(), s.hist.end(), complex<float>(0.0f, 0.0f));
  }

  const int n_out = capbuf.size();
  vector<complex<double> > tmp(n_out);
  soapy_decimate(s, n_out, &tmp[0]);
  double p = 0.0, pk = 0.0;
  for (int t = 0; t < n_out; t++) {
    capbuf(t) = tmp[t];
    const double m = tmp[t].real()*tmp[t].real() + tmp[t].imag()*tmp[t].imag();
    p += m;
    if (m > pk) pk = m;
  }
  // Signal-level readout to help diagnose antenna/gain issues (verbose only).
  if (verbosity >= 2) {
    fprintf(stderr, "[soapy] fc=%.3f MHz  mean_pwr=%.1f dB  peak_mag=%.3f\n",
            fc_programmed/1e6, 10.0*log10(p/n_out+1e-30), sqrt(pk));
  }
}

int soapy_read_u8(soapy_dev_t & s, unsigned char * out, int n_out)
{
  vector<complex<double> > tmp(n_out);
  soapy_decimate(s, n_out, &tmp[0]);
  for (int t = 0; t < n_out; t++) {
    int i = (int)lround(tmp[t].real() * 128.0 + 127.0);
    int q = (int)lround(tmp[t].imag() * 128.0 + 127.0);
    if (i < 0) i = 0; if (i > 255) i = 255;
    if (q < 0) q = 0; if (q > 255) q = 255;
    out[(t << 1)]     = (unsigned char)i;
    out[(t << 1) + 1] = (unsigned char)q;
  }
  return n_out;
}

void soapy_close(soapy_dev_t & s)
{
  if (s.dev && s.stream) {
    s.dev->deactivateStream(s.stream);
    s.dev->closeStream(s.stream);
    s.stream = 0;
  }
  if (s.dev) {
    SoapySDR::Device::unmake(s.dev);
    s.dev = 0;
  }
}

#endif // HAVE_SOAPYSDR
