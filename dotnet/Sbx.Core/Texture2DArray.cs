using System;

namespace Sbx.Core
{

  /**
   * A 2D texture array: one GPU image with a layer per texture, every layer the same size, format
   * and mip count (Godot's Texture2DArray). Shaders sample it from texture_arrays[] with
   * float3(uv, layer) -- each layer keeps its own mips and repeat wrap, unlike an atlas.
   *
   * Built from loaded textures (Create), copied on the GPU at the next upload: keep the layers
   * alive until IsResident. Load every layer with the same TextureFormat.
   */
  public sealed class Texture2DArray : Managed.INativeHandle, IDisposable
  {
    private readonly ulong _uuid;
    private bool _disposed;

    public ulong UUID => _uuid;

    ulong Managed.INativeHandle.Handle => _uuid;

    internal Texture2DArray(ulong uuid)
    {
      _uuid = uuid;
    }

    public static object? FromHandle(ulong handle) => handle != 0 ? new Texture2DArray(handle) : null;

    /**
     * An array of layers, layer i = layers[i]. Every layer must be resident and exactly
     * width x height, with the same format and mip count; otherwise null, and the engine log says
     * which layer doesn't fit.
     */
    public static Texture2DArray? Create(Texture2D[] layers, int width, int height)
    {
      if (layers.Length == 0 || width <= 0 || height <= 0)
      {
        return null;
      }

      var uuids = new ulong[layers.Length];

      for (var i = 0; i < layers.Length; i++)
      {
        uuids[i] = layers[i].UUID;
      }

      ulong uuid;

      unsafe
      {
        fixed (ulong* ptr = uuids)
        {
          uuid = InternalCalls.Texture2DArray_Create(ptr, (uint)uuids.Length, (uint)width, (uint)height);
        }
      }

      return uuid != 0 ? new Texture2DArray(uuid) : null;
    }

    /** Whether the layers have been copied in on the GPU; only then bind it to a material. */
    public bool IsResident
    {
      get { unsafe { return InternalCalls.Texture2DArray_IsResident(_uuid); } }
    }

    /** Frees the array's GPU image and bindless slot (not its layers). */
    public void Dispose()
    {
      if (_disposed)
      {
        return;
      }

      unsafe { InternalCalls.Texture2DArray_Release(_uuid); }

      _disposed = true;
    }

  } // class Texture2DArray

} // namespace Sbx.Core
