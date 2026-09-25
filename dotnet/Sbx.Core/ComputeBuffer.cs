using System;

namespace Sbx.Core
{

  public enum ComputeBufferAccess
  {
    /** Written by the script (SetData), read by shaders. */
    Upload = 0,
    /** Written by shaders, read back by the script (GetData) after ComputeCommands.Submit. */
    Readback = 1,
  }

  /**
   * A GPU buffer of T for a compute shader, bound to a pointer field of its push_data
   * (`my_struct* items;`) via ComputeShader.SetBuffer. T's size must equal the Slang struct's
   * stride -- checked when binding. Owns GPU memory, so Dispose it (or use `using`); there's no
   * finalizer, since native compute calls assume the script thread.
   */
  public sealed class ComputeBuffer<T> : IDisposable where T : unmanaged
  {
    private ulong _id;

    internal ulong Id => _id != 0 ? _id : throw new ObjectDisposedException(nameof(ComputeBuffer<T>));

    public int Count { get; }

    public ComputeBufferAccess Access { get; }

    /** count may be 0: the buffer still gets one element so it has a valid address to bind. */
    public unsafe ComputeBuffer(int count, ComputeBufferAccess access = ComputeBufferAccess.Upload)
    {
      _id = InternalCalls.ComputeBuffer_Create(count, sizeof(T), (uint)access);

      if (_id == 0)
      {
        throw new InvalidOperationException($"Failed to create ComputeBuffer<{typeof(T).Name}> with {count} elements, see the log");
      }

      Count = count;
      Access = access;
    }

    public unsafe void SetData(ReadOnlySpan<T> data)
    {
      if (data.Length > Count)
      {
        throw new ArgumentException($"{data.Length} elements exceed the buffer's {Count}", nameof(data));
      }

      fixed (T* ptr = data)
      {
        if (!InternalCalls.ComputeBuffer_SetData(Id, ptr, data.Length * sizeof(T)))
        {
          throw new InvalidOperationException("ComputeBuffer.SetData failed, see the log");
        }
      }
    }

    public unsafe void GetData(Span<T> destination)
    {
      if (Access != ComputeBufferAccess.Readback)
      {
        throw new InvalidOperationException("GetData needs a buffer created with ComputeBufferAccess.Readback");
      }

      if (destination.Length > Count)
      {
        throw new ArgumentException($"{destination.Length} elements exceed the buffer's {Count}", nameof(destination));
      }

      fixed (T* ptr = destination)
      {
        if (!InternalCalls.ComputeBuffer_GetData(Id, ptr, destination.Length * sizeof(T)))
        {
          throw new InvalidOperationException("ComputeBuffer.GetData failed, see the log");
        }
      }
    }

    public void Dispose()
    {
      if (_id == 0)
      {
        return;
      }

      unsafe { InternalCalls.ComputeBuffer_Release(_id); }

      _id = 0;
    }

  } // class ComputeBuffer

} // namespace Sbx.Core
