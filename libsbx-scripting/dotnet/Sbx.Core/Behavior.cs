using System;
using Sbx.Managed.Interop;

namespace Sbx.Core
{

  public abstract class Behavior
  {

    protected uint Node;
    private Dictionary<Type, Component> componentCache = new Dictionary<Type, Component>();

    protected Behavior() { Node = 0; }

		internal Behavior(uint node)
		{
			Node = node;
		}

		public virtual void OnCreate() { }

		public virtual void OnUpdate(float deltaTime) { }

		public virtual void OnFixedUpdate() { }

		public virtual void OnDestroy() { }


		public T? AddComponent<T>() where T : Component, new()
		{
			if (HasComponent<T>())
			{
				return GetComponent<T>();
			}

			unsafe { InternalCalls.Behavior_AddComponent(Node, typeof(T)); }

			var component = new T { Node = Node };

			componentCache.Add(typeof(T), component);

			return component;
		}

		public bool HasComponent<T>() where T : Component
		{
			unsafe { return InternalCalls.Behavior_HasComponent(Node, typeof(T)); }
		}

		public bool HasComponent(Type type)
		{
			unsafe { return InternalCalls.Behavior_HasComponent(Node, type); }
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
        var component = new T { Node = Node };
        
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

