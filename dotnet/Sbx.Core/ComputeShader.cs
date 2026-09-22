using System;

namespace Sbx.Core
{

  /**
   * A compute shader compiled at dispatch time from a project-relative .slang path (must declare
   * a `[shader("compute")] compute_main` entry point). Parameters are NOT resolved by name --
   * this engine's own compute work (the IBL baker) packs a plain ordered struct into push
   * constants rather than binding by reflection, so the Set* methods do the same: name exists
   * purely for readability at the call site, and each call appends its value, in call order, to
   * the pending dispatch's parameter block. The corresponding .slang file's own push_data struct
   * must declare its fields in that same order to match.
   *
   * One exception: SetBuffer occupies a single dedicated slot (at most one buffer per dispatch)
   * rather than sharing the ordered block, since a buffer's device address is 8 bytes against 4
   * for everything else -- see interop.hpp's ComputeShader section comment for why. A .slang
   * file's push_data struct must declare that address field first.
   *
   * A Set Dispatch cycle is build-then-fire: parameters clear after each Dispatch, so reusing
   * the same ComputeShader for another dispatch (e.g. one call per terrain chunk) means calling
   * Set* again first.
   *
   * IDisposable because it holds a native registry entry (see Dispose's own doc comment for what
   * that does and doesn't release) -- no finalizer, same reasoning as ComputeBuffer.
   */
  public sealed class ComputeShader : IDisposable
  {
    private ulong _id;
    private bool _disposed;

    private ComputeShader(ulong id)
    {
      _id = id;
    }

    public static ComputeShader? Load(string path)
    {
      ulong id;

      unsafe { id = InternalCalls.ComputeShader_Load(path); }

      return id != 0 ? new ComputeShader(id) : null;
    }

    /** Binds texture for reading (its sampled index). */
    public void SetTexture(string name, Texture2D texture)
    {
      unsafe { InternalCalls.ComputeShader_SetTexture(_id, texture.UUID); }
    }

    /** Binds texture as a UAV write target (its storage index) -- texture must come from Texture2D.CreateStorageImage. */
    public void SetOutputTexture(string name, Texture2D texture)
    {
      unsafe { InternalCalls.ComputeShader_SetOutputTexture(_id, texture.UUID); }
    }

    /** Sets this dispatch's one buffer slot -- see the class doc comment. */
    public void SetBuffer(string name, ComputeBuffer buffer)
    {
      unsafe { InternalCalls.ComputeShader_SetBuffer(_id, buffer.Id); }
    }

    public void SetFloat(string name, float value)
    {
      unsafe { InternalCalls.ComputeShader_SetFloat(_id, value); }
    }

    public void SetInt(string name, int value)
    {
      unsafe { InternalCalls.ComputeShader_SetInt(_id, value); }
    }

    /** Compiles (or reuses the cached compilation), binds the pending parameters, dispatches, and blocks until the GPU finishes. Clears the pending parameters afterward. */
    public void Dispatch(int groupsX, int groupsY, int groupsZ)
    {
      unsafe { InternalCalls.ComputeShader_Dispatch(_id, (uint)groupsX, (uint)groupsY, (uint)groupsZ); }
    }

    /** Drops this instance's registry entry. The compiled shader/pipeline themselves are cached engine-side by path, shared with every other user of it, and are not affected. */
    public void Dispose()
    {
      if (_disposed)
      {
        return;
      }

      unsafe { InternalCalls.ComputeShader_Release(_id); }

      _disposed = true;
      _id = 0;
    }

  } // class ComputeShader

} // namespace Sbx.Core
