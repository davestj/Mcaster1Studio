#pragma once

namespace M1 {

class SurfaceThreadPool;

/// Optional mixin interface for modules that need background thread pool access.
/// Modules implementing this receive their owning surface's dedicated thread pool
/// from MainWindow, allowing them to submit heavy work (encoding, transcription,
/// waveform generation, file I/O) to background threads without blocking the GUI.
///
/// Follows the same injection pattern as IDbAwareModule:
///   MainWindow does dynamic_cast<IThreadPoolAware*>(mod) and calls
///   setSurfaceThreadPool() after module creation.
///
/// If the pool is nullptr, modules should fall back to synchronous execution.
class IThreadPoolAware {
public:
    virtual ~IThreadPoolAware() = default;

    /// Called by MainWindow to inject the surface's thread pool.
    virtual void setSurfaceThreadPool(SurfaceThreadPool* pool) = 0;

    /// Returns the current surface thread pool (may be nullptr).
    virtual SurfaceThreadPool* surfaceThreadPool() const = 0;
};

} // namespace M1
