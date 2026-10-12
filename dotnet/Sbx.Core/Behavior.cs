using System;
using Sbx.Managed.Interop;
using Sbx.Core.Math;
using Sbx.Core.Physics;

namespace Sbx.Core
{
  public abstract class Behavior : Component
  {

    protected Behavior()
    {
      UUID = 0;
    }

    internal Behavior(ulong uuid)
    {
      UUID = uuid;
    }

    internal static T? GetBehavior<T>(ulong uuid) where T : Component, new()
    {
      var typeName = typeof(T).FullName;

      unsafe
      {
        var instance = InternalCalls.Scripting_GetInstance(uuid, typeName);

        return instance.Get() as T;
      }
    }

    protected Node? Node
    {
      get
      {
        return Node.Get(UUID);
      }
    }

    public T? AddComponent<T>() where T : Component, new()
    {
      return Node?.AddComponent<T>();
    }

    public bool HasComponent<T>() where T : Component
    {
      return Node != null && Node.HasComponent<T>();
    }

    public bool HasComponent(Type type)
    {
      return Node != null && Node.HasComponent(type);
    }

    public T? GetComponent<T>() where T : Component, new()
    {
      var componentType = typeof(T);

      if (typeof(Behavior).IsAssignableFrom(componentType))
      {
        return GetBehavior<T>(UUID);
      }

      return Node?.GetComponent<T>();
    }

    public virtual void OnCreate()
    {
      
    }

    public virtual void OnUpdate()
    {
      
    }

    public virtual void OnFixedUpdate()
    {
      
    }

    public virtual void OnDestroy()
    {
      
    }

    public virtual void OnCollisionEnter(Collision collision)
    {
      
    }

    public virtual void OnCollisionExit(Collision collision)
    {
      
    }

    public virtual void OnTriggerEnter(Collision collision)
    {
      
    }

    public virtual void OnTriggerExit(Collision collision)
    {
    }

    /** Called on a CharacterController's node for each surface its Move/SimpleMove ran into. */
    public virtual void OnControllerColliderHit(ControllerColliderHit hit)
    {

    }

    /** Called on every running script after a scene loads (after its own scripts' OnCreate). */
    public virtual void OnSceneLoaded(Scene scene, LoadSceneMode mode)
    {

    }

    /** Called on every running script after a scene unloads. */
    public virtual void OnSceneUnloaded(Scene scene)
    {

    }

    /** Called on every running script when the active scene changes. */
    public virtual void OnActiveSceneChanged(Scene previous, Scene current)
    {

    }

    public virtual void OnClick()
    {
      
    }

    public virtual void OnValueChanged()
    {

    }

    private void DispatchCollisionEnter(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
    {
      OnCollisionEnter(new Collision(Node.Get(otherUuid)!, new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
    }

    private void DispatchCollisionExit(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
    {
      OnCollisionExit(new Collision(Node.Get(otherUuid)!, new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
    }

    private void DispatchTriggerEnter(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
    {
      OnTriggerEnter(new Collision(Node.Get(otherUuid)!, new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
    }

    private void DispatchTriggerExit(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
    {
      OnTriggerExit(new Collision(Node.Get(otherUuid)!, new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
    }

    private void DispatchSceneLoaded(uint handle, uint mode)
    {
      OnSceneLoaded(new Scene(handle), (LoadSceneMode)mode);
    }

    private void DispatchSceneUnloaded(uint handle)
    {
      OnSceneUnloaded(new Scene(handle));
    }

    private void DispatchActiveSceneChanged(uint previous, uint current)
    {
      OnActiveSceneChanged(new Scene(previous), new Scene(current));
    }

    private void DispatchControllerColliderHit(ulong otherUuid, float pointX, float pointY, float pointZ, float normalX, float normalY, float normalZ, float directionX, float directionY, float directionZ, float moveLength)
    {
      OnControllerColliderHit(new ControllerColliderHit(Node.Get(otherUuid)!, new Vector3(pointX, pointY, pointZ), new Vector3(normalX, normalY, normalZ), new Vector3(directionX, directionY, directionZ), moveLength));
    }

  } // class Behavior

} // namespace Sbx.Core

