namespace Sbx.Core
{

  /**
   * A reference to a .material asset, carried across the native boundary as a raw uuid handle via
   * INativeHandle -- same convention as Node, since Sbx.Managed can't reference either type
   * directly (see Sbx.Managed.INativeHandle's own doc comment). Used as a [ShowInEditor] script
   * field type (drag-and-drop/asset-picker assignable, see inspector_script_section.cpp) and with
   * Components.MeshRenderer.SetMaterial to assign it to runtime-built geometry.
   */
  public sealed class Material : Managed.INativeHandle
  {
    private readonly ulong _uuid;

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

  } // class Material

} // namespace Sbx.Core
