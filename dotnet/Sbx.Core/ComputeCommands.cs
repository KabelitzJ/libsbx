using System;

namespace Sbx.Core
{

  /**
   * Records compute dispatches and submits them together with a single wait:
   *
   *   using var commands = new ComputeCommands();
   *   commands.Dispatch(first, groupsX, groupsY, 1);
   *   commands.Dispatch(second, groupsX, groupsY, 1);  // sees everything `first` wrote
   *   commands.Submit();
   *
   * Each Dispatch captures the shader's parameters at that moment, so the same shader can be
   * re-dispatched with different values. After Submit, results can be sampled, read with
   * Texture2D.ReadPixels or ComputeBuffer.GetData. Disposing without Submit discards the work.
   *
   * SubmitAsync doesn't block: poll IsComplete (e.g. once per Update) and only then read results.
   * Every buffer and texture the dispatches use must stay undisposed until IsComplete is true.
   * Disposing a still-running list waits for it.
   */
  public sealed class ComputeCommands : IDisposable
  {
    private ulong _id;

    private bool _submitted;

    public ComputeCommands()
    {
      unsafe { _id = InternalCalls.ComputeCommands_Begin(); }
    }

    /** True once a SubmitAsync'd list has finished on the GPU. Never blocks. */
    public bool IsComplete
    {
      get
      {
        if (_id == 0)
        {
          return true;
        }

        unsafe { return _submitted && InternalCalls.ComputeCommands_IsComplete(_id); }
      }
    }

    public void Dispatch(ComputeShader shader, int groupsX, int groupsY, int groupsZ)
    {
      if (_submitted)
      {
        throw new InvalidOperationException("ComputeCommands was already submitted");
      }

      bool ok;

      unsafe { ok = InternalCalls.ComputeCommands_Dispatch(Id, shader.Id, (uint)groupsX, (uint)groupsY, (uint)groupsZ); }

      if (!ok)
      {
        throw new InvalidOperationException($"ComputeCommands.Dispatch of '{shader.Path}' failed, see the log");
      }
    }

    /** Submits every recorded dispatch and blocks until the GPU finishes. The list can't be reused. */
    public void Submit()
    {
      SubmitImpl(true);

      _id = 0;
    }

    /** Submits every recorded dispatch without waiting -- see the class comment. */
    public void SubmitAsync()
    {
      SubmitImpl(false);
    }

    private void SubmitImpl(bool wait)
    {
      if (_submitted)
      {
        throw new InvalidOperationException("ComputeCommands was already submitted");
      }

      bool ok;

      unsafe { ok = InternalCalls.ComputeCommands_Submit(Id, wait); }

      _submitted = true;

      if (!ok)
      {
        throw new InvalidOperationException("ComputeCommands.Submit failed, see the log");
      }
    }

    public void Dispose()
    {
      if (_id == 0)
      {
        return;
      }

      unsafe { InternalCalls.ComputeCommands_Release(_id); }

      _id = 0;
    }

    private ulong Id => _id != 0 ? _id : throw new ObjectDisposedException(nameof(ComputeCommands), "already submitted or disposed");

  } // class ComputeCommands

} // namespace Sbx.Core
