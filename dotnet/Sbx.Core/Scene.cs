namespace Sbx.Core
{

  /**
   * A loaded scene, like Unity's Scene. Name, Path and BuildIndex are captured when the value is
   * created, so they stay readable after the scene unloads; IsLoaded is always live.
   */
  public readonly struct Scene : IEquatable<Scene>
  {
    internal const uint InvalidHandle = 0xFFFFFFFFu;

    public uint Handle { get; }
    public string Name { get; }
    public string Path { get; }
    /** Position in the project's scene list; -1 when the scene isn't in it. */
    public int BuildIndex { get; }

    internal Scene(uint handle)
    {
      Handle = handle;

      if (handle == InvalidHandle)
      {
        Name = string.Empty;
        Path = string.Empty;
        BuildIndex = -1;
        return;
      }

      string? name;
      string? path;

      unsafe
      {
        name = InternalCalls.Scene_GetName(handle);
        path = InternalCalls.Scene_GetPath(handle);
        BuildIndex = InternalCalls.Scene_GetBuildIndex(handle);
      }

      Name = name ?? string.Empty;
      Path = path ?? string.Empty;
    }

    /** False for a scene that was never found (e.g. GetSceneByName on a scene that isn't loaded). */
    public bool IsValid => Handle != InvalidHandle;

    public bool IsLoaded
    {
      get { unsafe { return IsValid && InternalCalls.Scene_IsLoaded(Handle); } }
    }

    /** The scene's top-level nodes. */
    public Node[] GetRootNodes()
    {
      if (!IsValid)
      {
        return Array.Empty<Node>();
      }

      Span<ulong> uuids = stackalloc ulong[64];

      var total = QueryRootNodes(uuids);

      if (total > uuids.Length)
      {
        uuids = new ulong[total];
        total = System.Math.Min(QueryRootNodes(uuids), uuids.Length);
      }

      var nodes = new Node[total];

      for (var i = 0; i < total; ++i)
      {
        nodes[i] = Node.Get(uuids[i])!;
      }

      return nodes;
    }

    private int QueryRootNodes(Span<ulong> uuids)
    {
      unsafe
      {
        fixed (ulong* uuidsPtr = uuids)
        {
          return (int)InternalCalls.Scene_GetRootNodes(Handle, uuidsPtr, (uint)uuids.Length);
        }
      }
    }

    public bool Equals(Scene other) => Handle == other.Handle;

    public override bool Equals(object? obj) => obj is Scene other && Equals(other);

    public override int GetHashCode() => Handle.GetHashCode();

    public static bool operator ==(Scene lhs, Scene rhs) => lhs.Equals(rhs);

    public static bool operator !=(Scene lhs, Scene rhs) => !lhs.Equals(rhs);

    public override string ToString() => IsValid ? Name : "(invalid scene)";

  } // struct Scene

  /** A queued scene load or unload, like Unity's AsyncOperation. Poll it, e.g. once per Update, to drive a loading screen. */
  public sealed class AsyncOperation
  {
    private readonly uint _id;
    private readonly uint _sceneHandle;

    internal AsyncOperation(uint id, uint sceneHandle)
    {
      _id = id;
      _sceneHandle = sceneHandle;
    }

    /** The scene being loaded or unloaded, looked up on each read: its name is known once a load is done. */
    public Scene Scene => new Scene(_sceneHandle);

    /** True once applied, or once rejected (an unknown scene, the last loaded scene, ...). */
    public bool IsDone
    {
      get { unsafe { return _id == 0 || InternalCalls.Scene_IsOperationDone(_id); } }
    }

    /** 0 to 1. An async load reaches 0.9 when its assets are ready, then waits for AllowSceneActivation; 1 once done. */
    public float Progress
    {
      get { unsafe { return _id == 0 ? 1.0f : InternalCalls.Scene_GetOperationProgress(_id); } }
    }

    /** False holds a loaded async scene at 0.9, e.g. until a loading screen's fade finishes. Also holds back async loads started after it. */
    public bool AllowSceneActivation
    {
      get { unsafe { return _id == 0 || InternalCalls.Scene_GetAllowActivation(_id); } }
      set { unsafe { InternalCalls.Scene_SetAllowActivation(_id, value ? 1u : 0u); } }
    }

  } // class AsyncOperation

} // namespace Sbx.Core
