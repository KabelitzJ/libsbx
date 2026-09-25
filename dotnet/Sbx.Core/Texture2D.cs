using System;

using Sbx.Core.Math;

namespace Sbx.Core
{

  /**
   * Pixel format for Texture2D.Load/CreateStorageImage/ReadPixels -- declaration order matches
   * interop::texture_load's and interop::texture_create_storage_image's own format arguments
   * exactly. Rgba8Srgb is Load-only: a Vulkan storage image can't be created in an sRGB format,
   * so CreateStorageImage rejects it (falls back to Rgba8, logging an error) the same way it
   * already rejects any other out-of-range value.
   */
  public enum TextureFormat
  {
    Rgba8Unorm = 0,
    R32Float = 1,
    R8 = 2,
    Rgba8Srgb = 3,
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

    /**
     * format defaults to Rgba8Srgb -- the engine's long-standing default for every loaded texture,
     * correct for photographic/authored color meant for gamma-correct display. Pass Rgba8 for
     * anything that isn't display color and must round-trip byte-for-byte instead -- a normal map
     * (already linear direction data), or a data channel like a heightmap/blend-mask atlas where
     * an sRGB decode would silently distort the actual numeric values, not just how it looks.
     */
    public static Texture2D? Load(string path, TextureFormat format = TextureFormat.Rgba8Srgb)
    {
      ulong uuid;

      unsafe { uuid = InternalCalls.Texture_Load(path, (uint)format); }

      return uuid != 0 ? new Texture2D(uuid) : null;
    }

    /** Allocates a new, empty texture usable as a ComputeShader storage texture, a sampled texture in a later dispatch, and a Material texture -- no layout transitions needed between those uses. */
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
