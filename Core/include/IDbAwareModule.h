#pragma once
#include "SurfaceDbContext.h"

namespace M1 {

/// Optional mixin interface for modules that need database access.
/// Modules implementing this receive per-surface DB context from MainWindow
/// before initialize() is called, allowing them to connect to the correct
/// database for their owning surface.
class IDbAwareModule {
public:
    virtual ~IDbAwareModule() = default;

    /// Called by MainWindow to inject the surface's database context.
    /// The module should store this and use it in initialize() to create
    /// its database connection via ctx.createConnection().
    virtual void setSurfaceDbContext(const SurfaceDbContext& ctx) = 0;

    /// Returns the current surface DB context (for inspection / re-connection).
    virtual SurfaceDbContext surfaceDbContext() const = 0;
};

} // namespace M1
