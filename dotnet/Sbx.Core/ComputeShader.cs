using System;

using Sbx.Core.Math;
using Sbx.Managed.Interop;

namespace Sbx.Core
{

  /**
   * A compute shader from a project-relative .slang path with a `[shader("compute")] compute_main`
   * entry point and a `[[vk::push_constant]] ConstantBuffer<push_data>`. Parameters are set by
   * push_data field name and type-checked against the field via reflection, so field order doesn't
   * matter. Texture/sampler fields use the handle types from `#include <script_compute.slang>`;
   * buffer fields are plain pointers. A sampler_handle field left unset gets a clamp-to-edge linear
   * sampler. Every other field must be set before the first Dispatch; values persist across
   * dispatches.
   *
   * Setters throw on an unknown field, a type mismatch, or a buffer whose element size differs
   * from the pointee's stride; the log has the details.
   */
  public sealed class ComputeShader : IDisposable
  {

    // Matches graphics::shader_compiler::push_constant_field::kind.
    private enum ValueKind : uint
    {
      Float = 0,
      Int = 1,
      UInt = 2,
      Float2 = 3,
      Float3 = 4,
      Float4 = 5,
    }

    private ulong _id;

    internal ulong Id => _id != 0 ? _id : throw new ObjectDisposedException(nameof(ComputeShader));

    public string Path { get; }

    private ComputeShader(ulong id, string path)
    {
      _id = id;
      Path = path;
    }

    /** Compiles and reflects the shader. Null (with the reason logged) if it's missing or doesn't compile. */
    public static ComputeShader? Load(string path)
    {
      ulong id;

      using (NativeString nativePath = path)
      {
        unsafe { id = InternalCalls.ComputeShader_Load(nativePath); }
      }

      return id != 0 ? new ComputeShader(id, path) : null;
    }

    public unsafe void SetFloat(string name, float value) => SetValue(name, ValueKind.Float, &value);

    public unsafe void SetInt(string name, int value) => SetValue(name, ValueKind.Int, &value);

    public unsafe void SetUInt(string name, uint value) => SetValue(name, ValueKind.UInt, &value);

    public unsafe void SetVector2(string name, Vector2 value) => SetValue(name, ValueKind.Float2, &value);

    public unsafe void SetVector3(string name, Vector3 value) => SetValue(name, ValueKind.Float3, &value);

    /** For a float4 field. */
    public unsafe void SetColor(string name, Color value) => SetValue(name, ValueKind.Float4, &value);

    /** For a sampled_texture field. Gate Load()'d textures on Texture2D.IsResident first. */
    public void SetTexture(string name, Texture2D texture) => SetTextureImpl(name, texture, false);

    /** For a storage_texture field; the texture must come from Texture2D.CreateStorageImage. */
    public void SetStorageTexture(string name, Texture2D texture) => SetTextureImpl(name, texture, true);

    public void SetBuffer<T>(string name, ComputeBuffer<T> buffer) where T : unmanaged
    {
      bool ok;

      using (NativeString nativeName = name)
      {
        unsafe { ok = InternalCalls.ComputeShader_SetBuffer(Id, nativeName, buffer.Id); }
      }

      ThrowIfFailed(ok, name);
    }

    public void Dispose()
    {
      if (_id == 0)
      {
        return;
      }

      unsafe { InternalCalls.ComputeShader_Release(_id); }

      _id = 0;
    }

    private unsafe void SetValue(string name, ValueKind kind, void* value)
    {
      bool ok;

      using (NativeString nativeName = name)
      {
        ok = InternalCalls.ComputeShader_SetValue(Id, nativeName, (uint)kind, value);
      }

      ThrowIfFailed(ok, name);
    }

    private void SetTextureImpl(string name, Texture2D texture, bool storage)
    {
      bool ok;

      using (NativeString nativeName = name)
      {
        unsafe { ok = InternalCalls.ComputeShader_SetTexture(Id, nativeName, texture.UUID, storage); }
      }

      ThrowIfFailed(ok, name);
    }

    private void ThrowIfFailed(bool ok, string name)
    {
      if (!ok)
      {
        throw new ArgumentException($"ComputeShader '{Path}': could not set '{name}', see the log", nameof(name));
      }
    }

  } // class ComputeShader

} // namespace Sbx.Core
