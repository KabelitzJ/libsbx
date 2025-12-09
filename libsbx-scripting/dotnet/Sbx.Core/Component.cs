using Sbx.Math;

namespace Sbx.Core
{

  public abstract class Component
  {
    public uint Node { get; internal set; }
  }

  public class Tag : Component
  {
    public string? Value
    {
      get { unsafe { return InternalCalls.Tag_GetTag(Node); } }
      set { unsafe { InternalCalls.Tag_SetTag(Node, value); } }
    }

    public override string ToString()
    {
      return Value ?? "[Unknown]";
    }
  } // public class Tag

	public class Transform : Component
	{
    public Vector3 Position
    {
      get {
        Vector3 position;
        unsafe { InternalCalls.Transform_GetPosition(Node, &position); }
        return position;
      }
      set { unsafe { InternalCalls.Transform_SetPosition(Node, &value); } }
    }

    public Vector3 Right
    {
      get {
        Vector3 right;
        unsafe { InternalCalls.Transform_GetRight(Node, &right); }
        return right;
      }
    }

    public Vector3 Forward
    {
      get {
        Vector3 forward;
        unsafe { InternalCalls.Transform_GetForward(Node, &forward); }
        return forward;
      }
    }

    public Vector3 Up
    {
      get {
        Vector3 up;
        unsafe { InternalCalls.Transform_GetUp(Node, &up); }
        return up;
      }
    }

		public Vector3 Rotation;
		public Vector3 Scale;

    public void LookAt(Vector3 target)
    {
      unsafe { InternalCalls.Transform_LookAt(Node, &target); }
    }

    public override bool Equals(object? obj) {
      return obj is Transform other && Equals(other);
    }

		public bool Equals(Transform other)     {
      return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
    }
    
		public override int GetHashCode()     {
      return (Position, Rotation, Scale).GetHashCode();
    }
    
		public static bool operator ==(Transform left, Transform right)     {
      return left.Equals(right);
    }
    
		public static bool operator !=(Transform left, Transform right)     {
      return !(left == right);
    }
    

	} // struct Transform

} // namespace Sbx.Core