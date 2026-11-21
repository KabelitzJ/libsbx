using Sbx.Managed.Interop;

namespace Sbx.Core
{

  /** The engine's built-in meshes, in primitive_mesh_kind's order. */
  public enum PrimitiveMesh : uint
  {
    Cube = 0,
    Sphere = 1,
    Plane = 2,
    Capsule = 3,
    Cylinder = 4,
  }

  /**
   * A mesh asset (an imported model or a built-in primitive). A shared, cached asset like AudioClip; nothing to dispose.
   * Usable as a script field: assign it in the Inspector like a Material.
   */
  public sealed class Mesh : Managed.INativeHandle
  {
    private readonly ulong _uuid;

    public ulong UUID => _uuid;

    ulong Managed.INativeHandle.Handle => _uuid;

    internal Mesh(ulong uuid)
    {
      _uuid = uuid;
    }

    public static object? FromHandle(ulong handle) => handle != 0 ? new Mesh(handle) : null;

    /** path is project-relative, e.g. "models/tree.glb". */
    public static Mesh? Load(string path)
    {
      ulong uuid;
      unsafe { uuid = InternalCalls.Mesh_Load(path); }
      return uuid != 0 ? new Mesh(uuid) : null;
    }

    public static Mesh Primitive(PrimitiveMesh kind)
    {
      ulong uuid;
      unsafe { uuid = InternalCalls.Mesh_Primitive((uint)kind); }
      return new Mesh(uuid);
    }

    /** False for a while after Load: the mesh streams in on a background thread. Instancing it before then simply draws once it's ready. */
    public bool IsLoaded
    {
      get { unsafe { return InternalCalls.Mesh_IsLoaded(_uuid); } }
    }

  } // class Mesh

} // namespace Sbx.Core
