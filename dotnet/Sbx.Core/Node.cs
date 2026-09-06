namespace Sbx.Core
{

  /**
   * A handle to a scene node other than (or the same as) the one a script is attached to --
   * Behavior itself only ever acts on its own node (see its AddComponent/GetComponent/HasComponent),
   * this is what lets a script reach any other node: find one by name, spawn a new one, reparent or
   * destroy one.
   *
   * Component access below reuses the exact same native calls Behavior.AddComponent<T>/HasComponent<T>
   * do -- they already take an explicit uuid, so nothing new was needed engine-side.
   */
  public sealed class Node
  {

    public ulong UUID { get; }

    public Node(ulong uuid)
    {
      UUID = uuid;
    }

    public string? Name
    {
      get { unsafe { return InternalCalls.Tag_GetTag(UUID); } }
      set { unsafe { InternalCalls.Tag_SetTag(UUID, value); } }
    }

    public T? GetComponent<T>() where T : Component, new()
    {
      if (!HasComponent<T>())
      {
        return null;
      }

      return new T { UUID = UUID };
    }

    public bool HasComponent<T>() where T : Component
    {
      unsafe { return InternalCalls.Behavior_HasComponent(UUID, typeof(T)); }
    }

    public T AddComponent<T>() where T : Component, new()
    {
      if (!HasComponent<T>())
      {
        unsafe { InternalCalls.Behavior_AddComponent(UUID, typeof(T)); }
      }

      return new T { UUID = UUID };
    }

    /** Destroys this node (and its subtree). Invokes OnDestroy on any of its own scripts first. */
    public void Destroy()
    {
      unsafe { InternalCalls.Node_Destroy(UUID); }
    }

    /** Pass null to move this node to the scene root. */
    public void SetParent(Node? parent)
    {
      unsafe { InternalCalls.Node_SetParent(UUID, parent?.UUID ?? 0); }
    }

    /** Null if no node with that name exists. */
    public static Node? Find(string name)
    {
      ulong uuid;
      unsafe { uuid = InternalCalls.Node_FindByName(name); }
      return uuid == 0 ? null : new Node(uuid);
    }

    /** A fresh, otherwise empty node (just a transform and this name) at the scene root. */
    public static Node Create(string name)
    {
      ulong uuid;
      unsafe { uuid = InternalCalls.Node_Create(name); }
      return new Node(uuid);
    }

  } // class Node

} // namespace Sbx.Core
