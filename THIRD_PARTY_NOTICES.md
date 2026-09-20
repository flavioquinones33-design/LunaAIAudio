# Third-party notices and provenance

Luna Audio AI is an independently authored prototype. It contains no Krisp code,
models, UI assets or branding. No affiliation or equivalent performance is claimed.

## RNNoise

- Official source: https://github.com/xiph/rnnoise
- Version: v0.1; commit `cdf196b1e9de2f8ff1003328ebf9a4316477429d`.
- The inference C source and original embedded `src/rnn_data.c` weights are unchanged.
- Upstream copyright/license text is retained in `third_party/rnnoise/COPYING` and
  source-file headers. The portable binary package includes those notices too.
- CMake integration is provided by this project. The selection of this older version
  is for a small reproducible baseline, not a claim that it is current or optimal.

Full top-level RNNoise license:

```text
Copyright (c) 2017, Mozilla
Copyright (c) 2007-2017, Jean-Marc Valin
Copyright (c) 2005-2017, Xiph.Org Foundation
Copyright (c) 2003-2004, Mark Borgerding

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.Org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FOUNDATION
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## miniaudio

- Official source: https://github.com/mackron/miniaudio
- Version: 0.11.23, vendored single header, unchanged.
- Download: https://raw.githubusercontent.com/mackron/miniaudio/0.11.23/miniaudio.h
- SHA-256: `7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6`.
- Upstream dual license: public domain or MIT No Attribution. License text is in
  `third_party/MINIAUDIO-LICENSE.txt` and the header, including notices of bundled
  subcomponents. MP3/FLAC, high-level engine and resource manager are not compiled.

## Build toolchain

The supplied Windows executables were cross-compiled with MinGW-w64. The GCC runtime
exception permits eligible application linking without changing the application's
license; runtime components keep their own notices. The portable package includes
the installed MinGW-w64/GCC package copyright notices in `licenses/toolchain`.
Native Windows builds can instead use MSVC as documented in README.

The prototype's original application source is delivered for the user's project;
no new public-distribution license is imposed on that original work here. Upstream
code retains its respective licenses. No third-party sources or licenses are
overridden by this document.
