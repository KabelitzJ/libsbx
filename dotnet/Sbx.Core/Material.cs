using System;

namespace Sbx.Core
{

  /** Which fixed texture slot to set via Material.SetTexture -- order matches interop::material_set_texture's slot argument exactly. */
  public enum MaterialTextureSlot
  {
    Albedo = 0,
    Normal = 1,
    MetallicRoughness = 2,
    Occlusion = 3,
    Emissive = 4,
  }

  /**
   * A reference to a .material asset, carried across the native boundary as a raw uuid handle via
   * INativeHandle -- same convention as Node, since Sbx.Managed can't reference either type
   * directly (see Sbx.Managed.INativeHandle's own doc comment). Used as a [ShowInEditor] script
   * field type (drag-and-drop/asset-picker assignable, see inspector_script_section.cpp) and with
   * Components.MeshRenderer.SetMaterial to assign it to runtime-built geometry.
   *
   * IDisposable for an instance made via CreateInstance: its slot in the engine's fixed-size
   * material buffer is never freed on its own (asset_residency::release_material's own doc
   * comment), so a script that creates one per generated object -- e.g. one terrain chunk
   * material per chunk -- must Dispose it when that object goes away, or it permanently consumes
   * one of a limited number of slots. Never call Dispose on a Load()'d template material still
   * used as the source for other instances. No finalizer, same reasoning as ComputeBuffer.
   */
  public sealed class Material : Managed.INativeHandle, IDisposable
  {
    private readonly ulong _uuid;
    private bool _disposed;

    public ulong UUID => _uuid;

    ulong Managed.INativeHandle.Handle => _uuid;

    internal Material(ulong uuid)
    {
      _uuid = uuid;
    }

    public static object? FromHandle(ulong handle) => handle != 0 ? new Material(handle) : null;

    public static Material? Load(string path)
    {
      ulong uuid;

      unsafe { uuid = InternalCalls.Material_Load(path); }

      return uuid != 0 ? new Material(uuid) : null;
    }

    /**
     * Whether this material's real file content (not just its handle) has been applied yet.
     * Always true for a CreateInstance-made material (its fields are copied synchronously at
     * creation). For a Load()'d material, false for a while after Load() returns -- check this
     * before CreateInstance-ing a Load()'d template, or the duplicate permanently copies whatever
     * placeholder/default fields the template happened to have at that instant (duplication is a
     * one-time synchronous field copy, not a live reference to the template).
     */
    public bool IsLoaded
    {
      get { unsafe { return InternalCalls.Material_IsLoaded(_uuid); } }
    }

    /**
     * Copies source into a brand-new, independently-registered material instance -- changes made
     * to the instance (SetTexture etc.) never affect source or any other instance made from it.
     * Use this before SetTexture whenever the material is going to be shared across multiple
     * pieces of generated content (e.g. one instance per terrain chunk) rather than a single
     * one-off.
     */
    public static Material? CreateInstance(Material source)
    {
      ulong uuid;

      unsafe { uuid = InternalCalls.Material_CreateInstance(source.UUID); }

      return uuid != 0 ? new Material(uuid) : null;
    }

    /** Overwrites one of this material's fixed texture slots in place -- see CreateInstance's own doc comment about sharing. */
    public void SetTexture(MaterialTextureSlot slot, Texture2D texture)
    {
      unsafe { InternalCalls.Material_SetTexture(_uuid, (uint)slot, texture.UUID); }
    }

    /** Frees this material's slot for reuse. Only call on an instance from CreateInstance -- never on a shared Load()'d template, see the class doc comment. */
    public void Dispose()
    {
      if (_disposed)
      {
        return;
      }

      unsafe { InternalCalls.Material_Release(_uuid); }

      _disposed = true;
    }

  } // class Material

} // namespace Sbx.Core
