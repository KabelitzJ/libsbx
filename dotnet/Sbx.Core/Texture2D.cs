using System;

using Sbx.Core.Math;

namespace Sbx.Core
{

  /** Pixel format for Texture2D.CreateStorageImage/ReadPixels -- declaration order matches interop::texture_create_storage_image's format argument exactly. */
  public enum TextureFormat
  {
    Rgba8 = 0,
    R32Float = 1,
    R8 = 2,
  }

  /**
   * A reference to a GPU-resident, bindless texture asset -- the real thing a Material can bind
   * (unlike the read-only Texture.SampleBilinear utility, which decodes a file into a CPU-only
   * cache and never touches the GPU).
   *
   * IDisposable for a texture created via CreateStorageImage: its bindless indices and GPU image
   * are never freed on their own (asset_residency::release_texture's own doc comment), so a
   * script that creates one -- e.g. a mixer/height bake target used only during a terrain chunk's
   * build -- must Dispose it once done, or it leaks for the life of the process. A file-loaded
   * texture from Load is a shared, cached asset (like Material.Load) and should never be
   * disposed -- Dispose is safe to skip entirely for those. No finalizer, same reasoning as
   * ComputeBuffer: native calls assume the script thread, not the GC finalizer thread.
   */
  public sealed class Texture2D : Managed.INativeHandle, IDisposable
  {
    private readonly ulong _uuid;
    private bool _disposed;

    public ulong UUID => _uuid;

    ulong Managed.INativeHandle.Handle => _uuid;

    internal Texture2D(ulong uuid)
    {
      _uuid = uuid;
    }

    public static object? FromHandle(ulong handle) => handle != 0 ? new Texture2D(handle) : null;

    public static Texture2D? Load(string path)
    {
      ulong uuid;

      unsafe { uuid = InternalCalls.Texture_Load(path); }

      return uuid != 0 ? new Texture2D(uuid) : null;
    }

    /** Allocates a new, empty texture registered both for sampling (a Material can read it) and as a compute UAV target -- zeroed and layout-ready to write into immediately. */
    public static Texture2D? CreateStorageImage(int width, int height, TextureFormat format)
    {
      ulong uuid;

      unsafe { uuid = InternalCalls.Texture_CreateStorageImage((uint)width, (uint)height, (uint)format); }

      return uuid != 0 ? new Texture2D(uuid) : null;
    }

    /**
     * Blocking GPU->CPU readback of every pixel, in row-major order. width/height/format must
     * match how this texture was created -- unlike CreateStorageImage's own call, the texture
     * doesn't remember them internally, so they're passed again here rather than tracked
     * natively. Single-channel formats (R32Float, R8) land in each Color's R component only.
     */
    public unsafe Color[] ReadPixels(int width, int height, TextureFormat format)
    {
      var pixels = new Color[width * height];

      fixed (Color* ptr = pixels)
      {
        InternalCalls.Texture_ReadPixels(_uuid, (uint)width, (uint)height, (uint)format, ptr);
      }

      return pixels;
    }

    /**
     * Whether this texture's pixel data has actually finished uploading to the GPU. Always true
     * for a CreateStorageImage texture. For a Load()'d texture, false for a while after Load()
     * returns -- the handle is valid immediately, but the real pixel data streams in on a
     * background thread and a per-frame upload budget. Check this (e.g. once per Update, not in a
     * blocking loop) before a compute shader samples a Load()'d texture, or it reads whatever
     * placeholder/empty data happened to be there at that instant.
     */
    public bool IsResident
    {
      get { unsafe { return InternalCalls.Texture_IsResident(_uuid); } }
    }

    /**
     * Transitions a CreateStorageImage texture so a Material can actually sample it correctly.
     * The image stays in a compute-writable layout for the whole time a ComputeShader is
     * writing to it (see CreateStorageImage's own doc comment) -- call this exactly once, after
     * the last Dispatch that writes to it and before the first Material.SetTexture that reads it,
     * or the material samples it through a mismatched layout (confirmed the hard way: the mesh
     * didn't render until this was added). No-op on a Load()'d texture.
     */
    public void PrepareForSampling()
    {
      unsafe { InternalCalls.Texture_PrepareForSampling(_uuid); }
    }

    /** Frees this texture's bindless indices and GPU image. Only call on an instance from CreateStorageImage -- never on a shared Load()'d texture, see the class doc comment. */
    public void Dispose()
    {
      if (_disposed)
      {
        return;
      }

      unsafe { InternalCalls.Texture_Release(_uuid); }

      _disposed = true;
    }

  } // class Texture2D

} // namespace Sbx.Core
