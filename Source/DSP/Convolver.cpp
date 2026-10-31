#include "DSP/Convolver.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace liminal::dsp {

namespace {
int ceilPow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}
int log2i(int n) {
    int o = 0;
    while ((1 << o) < n) ++o;
    return o;
}
}  // namespace

void Convolver::load(const float* ir, int len, double sampleRate, double maxSeconds) {
    length = std::max(1, std::min(len, int(maxSeconds * sampleRate)));
    std::vector<float> h(ir, ir + std::min(len, length));
    h.resize(std::size_t(length), 0.0f);
    if (len > length) {   // capped by Quality: fade out over the last 5 %
        const int fade = std::max(1, length / 20);
        for (int i = 0; i < fade; ++i) h[std::size_t(length - fade + i)] *= 0.5f + 0.5f * std::cos(3.14159265f * float(i) / float(fade));
    }

    head.assign(headSize, 0.0f);
    for (int i = 0; i < std::min(headSize, length); ++i) head[std::size_t(headSize - 1 - i)] = h[std::size_t(i)];
    headHistory.assign(2 * headSize, 0.0f);

    // Stage layout: partition P at offset 2P (stage 0: P = headSize at offset headSize).
    stages.clear();
    int offset = headSize, partition = headSize;
    while (offset < length) {
        Stage st;
        st.partition = partition;
        st.offset = offset;
        const int nextPartition = std::min(partition * growth, maxPartition);
        int end = (nextPartition > partition) ? 2 * nextPartition : length;
        if (nextPartition == partition) end = length;
        end = std::min(end, length);
        st.parts = std::max(1, (end - offset + partition - 1) / partition);
        st.fftSize = 2 * partition;
        st.slices = partition / headSize;
        const int index = int(stages.size());   // 0 for the first stage
        st.fftSlice = st.slices == 1 ? 0 : std::min(index, st.slices / 2 - 1);
        st.ifftSlice = st.slices == 1 ? 0 : st.slices - std::max(1, index);
        st.fft = std::make_unique<juce::dsp::FFT>(log2i(st.fftSize));
        const std::size_t bins = std::size_t(st.fftSize + 2);
        st.filters.assign(bins * std::size_t(st.parts), 0.0f);
        st.fdl.assign(bins * std::size_t(st.parts), 0.0f);
        st.frame.assign(std::size_t(2 * st.fftSize), 0.0f);
        st.acc.assign(bins, 0.0f);
        // Each partition's spectrum (zero-padded to 2P). JUCE's inverse real FFT divides by the
        // size, so the filters stay unscaled.
        std::vector<float> work(std::size_t(2 * st.fftSize));
        for (int k = 0; k < st.parts; ++k) {
            std::fill(work.begin(), work.end(), 0.0f);
            const int a = offset + k * partition;
            for (int i = 0; i < partition && a + i < length; ++i) work[std::size_t(i)] = h[std::size_t(a + i)];
            st.fft->performRealOnlyForwardTransform(work.data(), true);
            std::memcpy(&st.filters[bins * std::size_t(k)], work.data(), bins * sizeof(float));
        }
        offset += st.parts * partition;
        stages.push_back(std::move(st));
        partition = nextPartition;
    }

    const int maxP = stages.empty() ? headSize : stages.back().partition;
    const int ringIn = ceilPow2(2 * maxP + 8 * headSize);   // + the FFT slice's lag
    const int ringOut = ceilPow2(4 * maxP + 2 * headSize);
    inRing.assign(std::size_t(ringIn), 0.0f);
    outRing.assign(std::size_t(ringOut), 0.0f);
    inMask = ringIn - 1;
    outMask = ringOut - 1;
    reset();
}

void Convolver::reset() {
    std::fill(headHistory.begin(), headHistory.end(), 0.0f);
    std::fill(inRing.begin(), inRing.end(), 0.0f);
    std::fill(outRing.begin(), outRing.end(), 0.0f);
    for (auto& st : stages) {
        std::fill(st.fdl.begin(), st.fdl.end(), 0.0f);
        std::fill(st.acc.begin(), st.acc.end(), 0.0f);
        st.fdlHead = 0;
        st.macDone = 0;
    }
    headPos = 0;
    time = 0;
    blockFill = 0;
}

void Convolver::mac(Stage& st, int from, int to) {
    const std::size_t bins = std::size_t(st.fftSize + 2);
    float* acc = st.acc.data();
    for (int k = from; k < to; ++k) {
        // Partition k multiplies the input spectrum from k frames ago.
        const int slot = (st.fdlHead - k + st.parts) % st.parts;
        const float* x = &st.fdl[bins * std::size_t(slot)];
        const float* hk = &st.filters[bins * std::size_t(k)];
        for (std::size_t b = 0; b < bins; b += 2) {
            const float xr = x[b], xi = x[b + 1], hr = hk[b], hi = hk[b + 1];
            acc[b] += xr * hr - xi * hi;
            acc[b + 1] += xr * hi + xi * hr;
        }
    }
}

// One 128-sample slice of a stage's work, for the frame that ended at the last partition
// boundary T. Slice `fftSlice`: FFT the input [T - 2P, T) into the delay line (still in the input
// ring). The slices between: share the multiply-adds. Slice `ifftSlice`: the rest, the inverse
// FFT, and the last P samples go into the output ring for absolute times [T + offset - P,
// T + offset) - ahead of the reader, since offset >= 2P and ifftSlice < S. Stage s does its FFT
// in slice s and its inverse FFT in slice S - s, so no two stages' FFTs share a block.
void Convolver::runSlice(Stage& st, int slice) {
    const std::size_t bins = std::size_t(st.fftSize + 2);
    const int S = st.slices;
    if (slice == st.fftSlice) {
        st.fdlHead = (st.fdlHead + 1) % st.parts;
        float* f = st.frame.data();
        const long long start = time - (long long)st.fftSlice * headSize - 2 * st.partition;
        for (int i = 0; i < st.fftSize; ++i) f[i] = start + i >= 0 ? inRing[std::size_t((start + i) & inMask)] : 0.0f;
        std::fill(f + st.fftSize, f + 2 * st.fftSize, 0.0f);
        st.fft->performRealOnlyForwardTransform(f, true);
        std::memcpy(&st.fdl[bins * std::size_t(st.fdlHead)], f, bins * sizeof(float));
        std::fill(st.acc.begin(), st.acc.end(), 0.0f);
        st.macDone = 0;
        if (S == 1) {   // stage 0: everything now
            mac(st, 0, st.parts);
            st.macDone = st.parts;
        } else {
            return;
        }
    }
    if (slice < st.fftSlice) return;   // still the previous frame's quiet slices
    if (S > 1 && slice < st.ifftSlice) {
        // Share the partitions evenly across the slices between the two FFTs.
        const int workers = std::max(1, st.ifftSlice - st.fftSlice - 1);
        const int target = int((long long)st.parts * (slice - st.fftSlice) / workers);
        const int to = std::min(st.parts, target);
        if (to > st.macDone) {
            mac(st, st.macDone, to);
            st.macDone = to;
        }
        return;
    }
    if (slice > st.ifftSlice) return;
    if (st.macDone < st.parts) {
        mac(st, st.macDone, st.parts);
        st.macDone = st.parts;
    }
    float* f = st.frame.data();
    std::memcpy(f, st.acc.data(), bins * sizeof(float));
    std::fill(f + bins, f + 2 * st.fftSize, 0.0f);
    st.fft->performRealOnlyInverseTransform(f);
    // The frame was taken at t0 = time - (S-1) x headSize; its valid outputs are y_s[t0 - P, t0),
    // which land at absolute times + offset.
    const long long t0 = time - (long long)st.ifftSlice * headSize;
    const long long dst = t0 - st.partition + st.offset;
    for (int i = 0; i < st.partition; ++i) outRing[std::size_t((dst + i) & outMask)] += f[st.partition + i];
}

void Convolver::process(const float* in, float* out, int samples) {
    if (head.empty()) {
        if (in != out) std::memcpy(out, in, sizeof(float) * std::size_t(samples));
        return;
    }
    for (int n = 0; n < samples; ++n) {
        const float x = in[n];
        // Head: direct form against a mirrored history (contiguous headSize window).
        headHistory[std::size_t(headPos)] = x;
        headHistory[std::size_t(headPos + headSize)] = x;
        headPos = (headPos + 1) & (headSize - 1);
        const float* w = &headHistory[std::size_t(headPos)];   // oldest .. newest
        float y = 0.0f;
        for (int i = 0; i < headSize; ++i) y += w[i] * head[std::size_t(i)];
        inRing[std::size_t(time & inMask)] = x;
        float& o = outRing[std::size_t(time & outMask)];
        y += o;
        o = 0.0f;
        out[n] = y;
        ++time;
        if (++blockFill == headSize) {
            blockFill = 0;
            for (auto& st : stages) runSlice(st, int((time / headSize) % st.slices));
        }
    }
}

double Convolver::opsPerSample() const {
    double ops = headSize;
    for (const auto& st : stages) {
        const double fftOps = 2.0 * 2.5 * st.fftSize * std::log2(double(st.fftSize));   // forward + inverse
        ops += (fftOps + 4.0 * (st.fftSize / 2 + 1) * st.parts) / st.partition;
    }
    return ops;
}

void StereoConvolver::load(const std::vector<std::vector<float>>& channels, double sampleRate, int quality) {
    ok = !channels.empty() && !channels[0].empty();
    if (!ok) return;
    const double cap = Convolver::maxSecondsFor(quality);
    for (int c = 0; c < 2; ++c) {
        const auto& src = channels[std::size_t(std::min<int>(c, int(channels.size()) - 1))];
        ch[c].load(src.data(), int(src.size()), sampleRate, cap);
    }
    scratch.assign(8192, 0.0f);
}

void StereoConvolver::process(float* left, float* right, int samples, bool monoIn) {
    if (!ok) return;
    for (int done = 0; done < samples;) {
        const int n = std::min(samples - done, int(scratch.size()));
        if (monoIn) {
            for (int i = 0; i < n; ++i) scratch[std::size_t(i)] = 0.5f * (left[done + i] + right[done + i]);
            ch[0].process(scratch.data(), left + done, n);
            ch[1].process(scratch.data(), right + done, n);
        } else {
            ch[0].process(left + done, left + done, n);
            ch[1].process(right + done, right + done, n);
        }
        done += n;
    }
}

}  // namespace liminal::dsp
