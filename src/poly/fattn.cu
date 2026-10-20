// src/poly/fattn.cu - see the header.  The kernel is fattn_mma.cuh's, launched here in place of llama.cpp's
// launch_fattn(): one CUDA block a tile of 8 tokens and 8 heads, which attends to a whole block of keys, so the
// stream-k fixup is never needed and each tile leaves its softmax max and sum behind (fattn_mma.cuh, change 2).
#include "fattn.hpp"

#if !defined(GGML_USE_HIP)
#include "fattn_mma.cuh"
#endif

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <string>

namespace strata::poly {

#if !defined(GGML_USE_HIP)
namespace {

constexpr int kD = 256, kCols1 = 8, kCols2 = 8, kCols = kCols1 * kCols2;   // 8 tokens of a group's 8 heads a tile

const auto kKernel = flash_attn_ext_f16<kD, kD, kCols1, kCols2, false, false, false>;

struct Setup {
    bool ok = false;
    int nwarps = 0, nbatch_fa = 0;
    size_t smem = 0;
};

int grid(long long n, int per) { return (int) ((n + per - 1) / per); }

// The launch's shape and shared memory as ggml_cuda_flash_attn_ext_mma_f16_case() works them out, for the
// current card.  Ampere or later: on Turing the kernel has no 64-column tile.
Setup make_setup(int cc) {
    Setup r;
    if (!GGML_CUDA_CC_IS_NVIDIA(cc) || !ampere_mma_available(cc)) return r;
    const int nthreads = ggml_cuda_fattn_mma_get_nthreads(kD, kD, kCols, cc);
    const int nbatch_K2 = ggml_cuda_fattn_mma_get_nbatch_K2(kD, kD, kCols, cc);
    const int nbatch_V2 = ggml_cuda_fattn_mma_get_nbatch_V2(kD, kD, kCols, cc);
    const int nbatch_combine = ggml_cuda_fattn_mma_get_nbatch_combine(kD, kD, kCols, cc);
    const bool q_in_reg = ggml_cuda_fattn_mma_get_Q_in_reg(kD, kD, kCols, cc);
    const int nstages = ggml_cuda_fattn_mma_get_nstages(kD, kD, kCols1, kCols2, cc);
    r.nbatch_fa = ggml_cuda_fattn_mma_get_nbatch_fa(kD, kD, kCols, cc);
    r.nwarps = nthreads / WARP_SIZE;
    const int cols_per_warp = std::min(kCols, get_cols_per_warp(cc));
    // the tile's max and sum reach dst_meta only with one warp a query column (np == 1, fattn_mma.cuh change 2);
    // with more, the blocks of keys could not be merged
    if (r.nwarps * cols_per_warp != kCols) return r;
    const int stride_K = ggml_cuda_fattn_smem_swizzle::tile_stride(nbatch_K2, cc);
    const int stride_V = ggml_cuda_fattn_smem_swizzle::tile_stride(nbatch_V2, cc);
    const size_t kv1 = size_t(r.nbatch_fa) * std::max(stride_K, stride_V) * sizeof(half2);
    const size_t kv2 = size_t(r.nbatch_fa) * (stride_K + stride_V) * sizeof(half2);
    const size_t q = size_t(kCols) * (kD / 2 + 4) * sizeof(half2);
    const size_t mask = size_t(kCols1) * (r.nbatch_fa / 2 + 4) * sizeof(half2);
    const size_t combine = size_t(r.nwarps) * cols_per_warp * (nbatch_combine + 4) * sizeof(half2);
    const size_t kv = nstages <= 1 ? kv1 : kv2;
    r.smem = std::max(combine, q_in_reg ? std::max(q, kv + mask) : q + kv + mask);
    r.ok = cudaFuncSetAttribute(kKernel, cudaFuncAttributeMaxDynamicSharedMemorySize, (int) r.smem) == cudaSuccess;
    cudaGetLastError();
    return r;
}

// Each card's own, worked out the first time it asks: a layer split may put cards of different generations in one
// process, and cudaFuncSetAttribute() holds for the current card only.
const Setup& setup() {
    static std::mutex mu;
    static Setup s[GGML_CUDA_MAX_DEVICES];
    static bool done[GGML_CUDA_MAX_DEVICES] = {};
    const int d = ggml_cuda_get_device();
    std::lock_guard<std::mutex> lock(mu);
    if (!done[d]) {
        s[d] = make_setup(ggml_cuda_info().devices[d].cc);
        done[d] = true;
    }
    return s[d];
}

// The scratch for a batch of n tokens: a block's results and their max and sum, the merged max and sum, the
// causal block's steps, then K and V side by side, each piece 16-byte aligned (offsets in floats).  rows: the keys
// a block takes, a whole number of steps and at most a grid's 65535 rows (dequant_kernel); 0 when the scratch
// cannot hold a block of n + nbatch_fa rows.
struct Layout {
    size_t part = 0, pmeta = 0, ameta = 0, kv_max = 0, kv = 0;
    long long rows = 0;
};

Layout layout(int n, int dim, int n_head, int kv_heads, int nbatch_fa, size_t scratch_floats) {
    Layout L;
    const size_t heads = size_t(n) * n_head;
    size_t at = 0;
    auto take = [&](size_t floats) {
        const size_t p = at;
        at += (floats + 3) & ~size_t(3);
        return p;
    };
    L.part = take(heads * dim);
    L.pmeta = take(2 * heads);
    L.ameta = take(2 * heads);
    L.kv_max = take((size_t) grid(n, kCols1));
    L.kv = at;
    if (at >= scratch_floats) return L;
    // a row of K and one of V take kv_heads * dim floats together
    const size_t width = size_t(kv_heads) * dim;
    const long long rows =
        std::min<long long>((long long) ((scratch_floats - at) / width), 65535) / nbatch_fa * nbatch_fa;
    if (rows >= (long long) n + nbatch_fa) L.rows = rows;
    return L;
}

void ck(const char* what) {
    const cudaError_t e = cudaGetLastError();
    if (e != cudaSuccess) throw std::runtime_error(std::string("fattn: ") + what + ": " + cudaGetErrorString(e));
}

// rows [row, row + valid) of a q8_0 cache (or an fp16 one, f16) as fp16, and zeros on to `blockIdx.y`'s last row:
// the kernel reads whole steps of keys, and the causal mask removes the zero rows' weights, but a NaN would get
// through it
__global__ void dequant_kernel(const int8_t* __restrict__ codes, const uint16_t* __restrict__ scales,
                               const half* __restrict__ f16, half* __restrict__ out, int width, long long row,
                               int valid) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= width) return;
    const int r = blockIdx.y;
    half x = __float2half(0.0f);
    if (r < valid) {
        const size_t at = size_t(row + r) * width;
        x = f16 ? f16[at + i]
                : __float2half((float) codes[at + i] * __half2float(__ushort_as_half(scales[at / 32 + i / 32])));
    }
    out[size_t(r) * width + i] = x;
}

// The last step of keys a tile of tokens reads in the causal block: token j sees the keys up to j + delta.
__global__ void kv_max_kernel(int* __restrict__ kv_max, int tiles, int n, long long delta, int keys, int step) {
    const int t = blockIdx.x * blockDim.x + threadIdx.x;
    if (t >= tiles) return;
    const int j = t * kCols1 + kCols1 - 1 < n ? t * kCols1 + kCols1 - 1 : n - 1;
    const long long seen = (j + delta + 1 + step - 1) / step * step;
    kv_max[t] = seen < keys ? (int) seen : keys;
}

// A block of keys' results into what the blocks before it made of a token and head: each normalized by its own
// softmax sum, so each weighs in by its sum against the larger max.  One CUDA block a token and head.
__global__ void merge_kernel(float* __restrict__ out, float2* __restrict__ meta, const float* __restrict__ part,
                             const float2* __restrict__ pmeta, int dim) {
    const size_t row = blockIdx.x;
    const float2 a = meta[row], b = pmeta[row];
    const float top = fmaxf(a.x, b.x), wa = a.y * __expf(a.x - top), wb = b.y * __expf(b.x - top);
    const float inv = 1.0f / (wa + wb);
    for (int d = threadIdx.x; d < dim; d += blockDim.x)
        out[row * dim + d] = (out[row * dim + d] * wa + part[row * dim + d] * wb) * inv;
    __syncthreads();
    if (threadIdx.x == 0) meta[row] = make_float2(top, wa + wb);
}

// The q8 cache (kq8, vq8) or the fp16 one (keys, vals): an fp16 cache's whole steps before pos are read where they
// lie, in one block, and only the last block is copied into the scratch.
bool run(const float* q, Kvq8 kq8, Kvq8 vq8, const half* keys, const half* vals, int64_t pos, int n, float* out,
         int dim, int n_head, int kv_heads, float scale, float* scratch, size_t scratch_floats, void* stream) {
    if (n <= 0 || !fattn_fits(dim, n_head, kv_heads, n, scratch_floats)) return false;
    const Setup& S = setup();
    const int width = kv_heads * dim, tiles = grid(n, kCols1);
    const size_t heads = size_t(n) * n_head;
    const Layout L = layout(n, dim, n_head, kv_heads, S.nbatch_fa, scratch_floats);
    if (L.rows == 0) return false;
    const long long rows = L.rows;
    float* part = scratch + L.part;
    float2* pmeta = (float2*) (scratch + L.pmeta);
    float2* ameta = (float2*) (scratch + L.ameta);
    int* kv_max = (int*) (scratch + L.kv_max);
    half* kw = (half*) (scratch + L.kv);
    half* vw = kw + size_t(rows) * width;

    const cudaStream_t s = (cudaStream_t) stream;
    const dim3 block(WARP_SIZE, S.nwarps);
    const uint3 ne01 = init_fastdiv_values((uint64_t) n);
    const int32_t nb01 = n_head * dim * (int) sizeof(float), nb02 = dim * (int) sizeof(float);
    const int32_t nb11 = width * (int) sizeof(half), nb12 = dim * (int) sizeof(half);
    const long long total = pos + n;
    const long long whole = pos / S.nbatch_fa * S.nbatch_fa;   // an fp16 cache's rows read in place
    for (long long k0 = 0, b = 0; k0 < total; ++b) {
        // Blocks of whole steps from the start, as long as what is left is more than a block; then the rest,
        // which holds the batch's own rows and starts at or before pos, so that every token sees a key of it.
        const bool last = keys ? k0 == whole : total - k0 <= rows;
        const long long m = last ? total - k0
                          : keys ? whole
                                 : std::min<long long>(rows, (pos - k0) / S.nbatch_fa * S.nbatch_fa);
        const int nk = grid(m, S.nbatch_fa) * S.nbatch_fa;
        const long long delta = pos - k0;
        const half* kb = kw;
        const half* vb = vw;
        if (keys && !last) {
            kb = keys, vb = vals;
        } else {
            dequant_kernel<<<dim3(grid(width, 256), nk), 256, 0, s>>>(kq8.codes, kq8.scales, keys, kw, width, k0,
                                                                      (int) m);
            dequant_kernel<<<dim3(grid(width, 256), nk), 256, 0, s>>>(vq8.codes, vq8.scales, vals, vw, width, k0,
                                                                      (int) m);
        }
        if (last) kv_max_kernel<<<grid(tiles, 128), 128, 0, s>>>(kv_max, tiles, n, delta, nk, S.nbatch_fa);
        float* dst = b == 0 ? out : part;
        float2* meta = b == 0 ? ameta : pmeta;
        kKernel<<<tiles * kv_heads, block, S.smem, s>>>(
            (const char*) q, (const char*) kb, (const char*) vb, nullptr, nullptr, last ? kv_max : nullptr, dst, meta,
            scale, 0.0f, 1.0f, 1.0f, 1u, 0.0f,
            dim, ne01, n_head, 1, nb01, nb02, nb01 * n,
            dim, nk, kv_heads, 1, nb11, nb12, (int64_t) nb11 * nk,
            nb11, nb12, (int64_t) nb11 * nk,
            n, 1, 1, (int32_t) (2 * delta), 0, 0);
        ck("attention");
        if (b > 0) merge_kernel<<<(unsigned) heads, dim, 0, s>>>(out, ameta, part, pmeta, dim);
        k0 += m;
    }
    ck("merge");
    return true;
}

}  // namespace

bool fattn_fits(int dim, int n_head, int kv_heads, int rows, size_t scratch_floats) {
    if (dim != kD || kv_heads <= 0 || n_head != kCols2 * kv_heads || rows <= 0) return false;
    const Setup& S = setup();
    return S.ok && layout(rows, dim, n_head, kv_heads, S.nbatch_fa, scratch_floats).rows > 0;
}

bool fattn_q8(const float* q, Kvq8 kq8, Kvq8 vq8, int64_t pos, int n, float* out, int dim, int n_head, int kv_heads,
              float scale, float* scratch, size_t scratch_floats, void* stream) {
    return kq8.codes && vq8.codes &&
           run(q, kq8, vq8, nullptr, nullptr, pos, n, out, dim, n_head, kv_heads, scale, scratch, scratch_floats,
               stream);
}

bool fattn_f16(const float* q, const uint16_t* keys, const uint16_t* vals, int64_t pos, int n, float* out, int dim,
               int n_head, int kv_heads, float scale, float* scratch, size_t scratch_floats, void* stream) {
    return keys && vals &&
           run(q, Kvq8{}, Kvq8{}, (const half*) keys, (const half*) vals, pos, n, out, dim, n_head, kv_heads, scale,
               scratch, scratch_floats, stream);
}

#else

bool fattn_fits(int, int, int, int, size_t) { return false; }

bool fattn_q8(const float*, Kvq8, Kvq8, int64_t, int, float*, int, int, int, float, float*, size_t, void*) {
    return false;
}

bool fattn_f16(const float*, const uint16_t*, const uint16_t*, int64_t, int, float*, int, int, int, float, float*,
               size_t, void*) {
    return false;
}

#endif

}  // namespace strata::poly
