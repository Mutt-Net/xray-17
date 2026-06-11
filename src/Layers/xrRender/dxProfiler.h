// dxProfiler.h — lightweight, runtime-gated GPU phase profiler.
//
// Header-only (all methods inline; single global defined in dxRenderDeviceRender.cpp)
// so it compiles into every renderer target (R4/R5/R5VK) without cmake source-list edits.
//
// Mechanism: D3D11 timestamp queries (D3D11_QUERY_TIMESTAMP + _DISJOINT). These work
// natively on DX11 and through the D3D11On12 bridge (DX12) and the VK offline-RT path,
// so one instrument profiles all three backends identically.
//
// Usage:
//   GPU_ZONE("Lights");            // RAII scope inside a render phase
//   GPUProfiler.FrameBegin(dev,ctx) / FrameEnd(frameSec)  // once per frame
// Enable at runtime with the "-gpuprofile" command-line switch. Off => ~zero cost.
//
// Design notes:
//   * Zone 0 is the implicit whole-frame zone ("FRAME") => total GPU ms.
//   * Queries are read back FRAME_SLOTS frames later (async) to avoid CPU/GPU stalls.
//   * Zones are keyed by name; a name fired multiple times in a frame accumulates.
//   * Bounded: MAX_ZONES timestamp pairs per slot; per-frame zones only (no inner loops).

#pragma once

#include <d3d11.h>

class dxGPUProfiler
{
public:
    static constexpr int MAX_ZONES     = 24;
    static constexpr int FRAME_SLOTS   = 5;   // async readback depth
    static constexpr int ZONE_NAME_LEN = 28;

    bool Enabled() const { return m_enabled; }
    void SetEnabled(bool e) { m_enabled = e; }

    // Lazily creates the query pool on first use; safe to call every frame.
    void FrameBegin(ID3D11Device* dev, ID3D11DeviceContext* ctx)
    {
        if (!m_enabled) return;
        if (!m_inited) Init(dev);
        if (!m_inited) { m_enabled = false; return; } // creation failed -> disable

        m_ctx = ctx;
        m_cur = m_frame % FRAME_SLOTS;
        Slot& s = m_slot[m_cur];

        // Read back the data this slot still holds (from FRAME_SLOTS frames ago) before reuse.
        if (s.pending) CollectSlot(s);

        for (int i = 0; i < MAX_ZONES; ++i) s.used[i] = false;
        ctx->Begin(s.disjoint);
        m_ctx->End(s.tsBegin[0]); // implicit whole-frame zone start
        s.used[0] = true;
    }

    int ZoneBegin(const char* name)
    {
        if (!m_enabled || !m_ctx) return -1;
        int idx = ZoneIndex(name);
        if (idx < 0) return -1;
        Slot& s = m_slot[m_cur];
        m_ctx->End(s.tsBegin[idx]);
        s.used[idx] = true;
        return idx;
    }

    void ZoneEnd(int idx)
    {
        if (!m_enabled || !m_ctx || idx < 0) return;
        m_ctx->End(m_slot[m_cur].tsEnd[idx]);
    }

    // frameSec = wall-clock seconds for the frame (Device.fTimeDelta) for the CPU/FPS line.
    // Returns true on the frame it emits a report (so the caller can append a resource line).
    bool FrameEnd(float frameSec)
    {
        if (!m_enabled || !m_ctx) return false;
        Slot& s = m_slot[m_cur];
        m_ctx->End(s.tsEnd[0]);   // whole-frame zone end
        m_ctx->End(s.disjoint);
        s.pending = true;
        ++m_frame;

        m_accCPUms += frameSec * 1000.0f;
        m_reportTimer += frameSec;
        if (m_reportTimer >= 1.0f) { EmitReport(); return true; }
        return false;
    }

    void Shutdown()
    {
        for (auto& s : m_slot)
        {
            if (s.disjoint) { s.disjoint->Release(); s.disjoint = nullptr; }
            for (int i = 0; i < MAX_ZONES; ++i)
            {
                if (s.tsBegin[i]) { s.tsBegin[i]->Release(); s.tsBegin[i] = nullptr; }
                if (s.tsEnd[i])   { s.tsEnd[i]->Release();   s.tsEnd[i]   = nullptr; }
            }
        }
        m_inited = false;
    }

private:
    struct Slot
    {
        ID3D11Query* disjoint = nullptr;
        ID3D11Query* tsBegin[MAX_ZONES] = {};
        ID3D11Query* tsEnd[MAX_ZONES]   = {};
        bool used[MAX_ZONES] = {};
        bool pending = false;
    };

    void Init(ID3D11Device* dev)
    {
        D3D11_QUERY_DESC dj{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
        D3D11_QUERY_DESC ts{ D3D11_QUERY_TIMESTAMP, 0 };
        for (auto& s : m_slot)
        {
            if (FAILED(dev->CreateQuery(&dj, &s.disjoint))) return;
            for (int i = 0; i < MAX_ZONES; ++i)
            {
                if (FAILED(dev->CreateQuery(&ts, &s.tsBegin[i]))) return;
                if (FAILED(dev->CreateQuery(&ts, &s.tsEnd[i])))   return;
            }
        }
        xr_strcpy(m_zoneName[0], "FRAME");
        m_numZones = 1;
        m_inited = true;
    }

    int ZoneIndex(const char* name)
    {
        for (int i = 0; i < m_numZones; ++i)
            if (0 == xr_strcmp(m_zoneName[i], name)) return i;
        if (m_numZones >= MAX_ZONES) return -1;
        xr_strcpy(m_zoneName[m_numZones], name);
        return m_numZones++;
    }

    void CollectSlot(Slot& s)
    {
        s.pending = false;
        const UINT noflush = D3D11_ASYNC_GETDATA_DONOTFLUSH;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
        if (m_ctx->GetData(s.disjoint, &dj, sizeof(dj), noflush) != S_OK) return; // not ready -> drop
        if (dj.Disjoint || dj.Frequency == 0) return;

        for (int z = 0; z < m_numZones; ++z)
        {
            if (!s.used[z]) continue;
            UINT64 t0 = 0, t1 = 0;
            if (m_ctx->GetData(s.tsBegin[z], &t0, sizeof(t0), noflush) != S_OK) continue;
            if (m_ctx->GetData(s.tsEnd[z],   &t1, sizeof(t1), noflush) != S_OK) continue;
            if (t1 > t0) m_accMs[z] += double(t1 - t0) * 1000.0 / double(dj.Frequency);
        }
        ++m_accFrames;
    }

    void EmitReport()
    {
        if (m_accFrames == 0) { m_reportTimer = 0.f; return; }

        const double inv = 1.0 / double(m_accFrames);
        const double frameGpu = m_accMs[0] * inv;
        const float  cpuMs    = m_accCPUms / float(m_accFrames);
        const float  fps      = cpuMs > 0.f ? 1000.0f / cpuMs : 0.f;

        Msg("* GPUPROF: %.1f fps | frame %.2f ms (CPU) | GPU %.2f ms | %u samples",
            fps, cpuMs, frameGpu, m_accFrames);

        // Per-zone GPU ms, sorted high->low (skip zone 0 = whole frame).
        int order[MAX_ZONES];
        int n = 0;
        for (int z = 1; z < m_numZones; ++z) order[n++] = z;
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b)
                if (m_accMs[order[b]] > m_accMs[order[a]]) { int t = order[a]; order[a] = order[b]; order[b] = t; }

        for (int i = 0; i < n; ++i)
        {
            const int z = order[i];
            const double ms = m_accMs[z] * inv;
            const double pct = frameGpu > 0.0 ? (ms / frameGpu) * 100.0 : 0.0;
            Msg("* GPUPROF:   %-16s %6.2f ms  (%4.1f%%)", m_zoneName[z], ms, pct);
        }

        for (int z = 0; z < MAX_ZONES; ++z) m_accMs[z] = 0.0;
        m_accFrames  = 0;
        m_accCPUms   = 0.f;
        m_reportTimer = 0.f;
    }

    bool                 m_enabled = false;
    bool                 m_inited  = false;
    ID3D11DeviceContext* m_ctx     = nullptr;

    Slot m_slot[FRAME_SLOTS];
    char m_zoneName[MAX_ZONES][ZONE_NAME_LEN] = {};
    int  m_numZones = 0;

    u32    m_frame       = 0;
    int    m_cur         = 0;
    double m_accMs[MAX_ZONES] = {};
    u32    m_accFrames   = 0;
    float  m_accCPUms    = 0.f;
    float  m_reportTimer = 0.f;
};

extern dxGPUProfiler GPUProfiler;

// RAII zone marker — near-zero cost when profiling is disabled.
struct dxGPUZoneScope
{
    int idx;
    explicit dxGPUZoneScope(const char* name)
        : idx(GPUProfiler.Enabled() ? GPUProfiler.ZoneBegin(name) : -1) {}
    ~dxGPUZoneScope() { if (idx >= 0) GPUProfiler.ZoneEnd(idx); }
};

#define GPU_ZONE_CAT2(a, b) a##b
#define GPU_ZONE_CAT(a, b)  GPU_ZONE_CAT2(a, b)
#define GPU_ZONE(name)      dxGPUZoneScope GPU_ZONE_CAT(_gpuZone_, __LINE__)(name)
