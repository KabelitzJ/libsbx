using System;
using Sbx.Managed.Interop;
using Sbx.Math;

namespace Sbx.Core
{
  public abstract class Behavior
  {
    protected ulong UUID;
    private Dictionary<Type, Component> componentCache = new Dictionary<Type, Component>();

    protected Behavior() { UUID = 0; }

		internal Behavior(ulong uuid)
		{
			UUID = uuid;
		}

		public virtual void OnCreate() { }

		public virtual void OnUpdate() { }

		public virtual void OnFixedUpdate() { }

		public virtual void OnDestroy() { }

		public virtual void OnCollisionEnter(Collision collision) { }

		public virtual void OnCollisionExit(Collision collision) { }

		public virtual void OnTriggerEnter(Collision collision) { }

		public virtual void OnTriggerExit(Collision collision) { }

		/**
		 * This node -- for reaching Node's Find/Create/Destroy/SetParent/GetComponent<T> surface on
		 * yourself, symmetrically with how you'd call it on any other node.
		 */
		protected Node Node => new Node(UUID);

		// Below: invoked directly by native code (scripting_module::_invoke_collision_handler) by
		// name via reflection -- see managed::object::invoke. Public rather than private specifically
		// so that invocation doesn't depend on a NonPublic reflection binding flag being set on the
		// native side; not meant to be called directly by script code (call the OnX overrides above
		// instead, which these forward into after building the friendlier Collision payload).
		public void __DispatchCollisionEnter(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
		{
			OnCollisionEnter(new Collision(new Node(otherUuid), new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
		}

		public void __DispatchCollisionExit(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
		{
			OnCollisionExit(new Collision(new Node(otherUuid), new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
		}

		public void __DispatchTriggerEnter(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
		{
			OnTriggerEnter(new Collision(new Node(otherUuid), new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
		}

		public void __DispatchTriggerExit(ulong otherUuid, float normalX, float normalY, float normalZ, float pointX, float pointY, float pointZ)
		{
			OnTriggerExit(new Collision(new Node(otherUuid), new Vector3(normalX, normalY, normalZ), new Vector3(pointX, pointY, pointZ)));
		}

		public T? AddComponent<T>() where T : Component, new()
		{
			if (HasComponent<T>())
			{
				return GetComponent<T>();
			}

			unsafe { InternalCalls.Behavior_AddComponent(UUID, typeof(T)); }

			var component = new T { UUID = UUID };

			componentCache.Add(typeof(T), component);

			return component;
		}

		public bool HasComponent<T>() where T : Component
		{
			unsafe { return InternalCalls.Behavior_HasComponent(UUID, typeof(T)); }
		}

		public bool HasComponent(Type type)
		{
			unsafe { return InternalCalls.Behavior_HasComponent(UUID, type); }
		}

		public T? GetComponent<T>() where T : Component, new()
		{
			Type componentType = typeof(T);

			if (!HasComponent<T>())
			{
        componentCache.Remove(componentType);

				return null;
			}

			if (!componentCache.ContainsKey(componentType))
      {
        var component = new T { UUID = UUID };
        
				componentCache.Add(componentType, component);

				return component;
			}

      return componentCache[componentType] as T;
		}

		// public bool RemoveComponent<T>() where T : Component
		// {
		// 	Type componentType = typeof(T);
    //   bool removed = false;

		// 	unsafe { removed = InternalCalls.Behavior_RemoveComponent(node, componentType); }

		// 	if (removed && componentCache.ContainsKey(componentType))
    //   {
		// 		componentCache.Remove(componentType);
    //   }

		// 	return removed;
		// }
  } // class Behavior

} // namespace Sbx.Core

