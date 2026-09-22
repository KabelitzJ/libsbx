using System;

namespace Sbx.Core
{

  /**
   * A GPU-resident structured buffer for a compute shader to read -- the script-side counterpart
   * of a vertex/index buffer, but for arbitrary data (e.g. one struct per hex). Purely a runtime
   * resource: no uuid/content-asset identity, not inspector-assignable, so unlike Material/
   * Texture2D it isn't an INativeHandle. Owns real GPU memory, so it's IDisposable -- call
   * Dispose (or wrap it in a `using`) once you're done with it rather than letting it sit forever;
   * there's no finalizer chasing it down, since finalizers run on the GC's own thread and this
   * engine's native compute calls assume the script/main thread.
   */
  public sealed class ComputeBuffer : IDisposable
  {
    private ulong _id;
    private bool _disposed;

    internal ulong Id => _id;

    public ComputeBuffer(int count, int stride)
    {
      unsafe { _id = InternalCalls.ComputeBuffer_Create(count, stride); }
    }

    /** Uploads data in full, overwriting the buffer's previous contents. T must match the stride passed to the constructor. */
    public unsafe void SetData<T>(T[] data) where T : unmanaged
    {
      if (_disposed)
      {
        throw new ObjectDisposedException(nameof(ComputeBuffer));
      }

      fixed (T* ptr = data)
      {
        InternalCalls.ComputeBuffer_SetData(_id, ptr, data.Length * sizeof(T));
      }
    }

    public void Dispose()
    {
      if (_disposed)
      {
        return;
      }

      unsafe { InternalCalls.ComputeBuffer_Release(_id); }

      _disposed = true;
      _id = 0;
    }

  } // class ComputeBuffer

} // namespace Sbx.Core
