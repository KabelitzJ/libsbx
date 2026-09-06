using Sbx.Math;

namespace Sbx.Core
{

  public abstract class Component
  {

    public ulong UUID { get; internal set; }

  }

  public class Tag : Component
  {

    public string? Value
    {
      get 
      { 
        unsafe { return InternalCalls.Tag_GetTag(UUID); } 
      }
      set 
      { 
        unsafe { InternalCalls.Tag_SetTag(UUID, value); } 
      }
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
      get 
      {
        Vector3 position;
        unsafe { InternalCalls.Transform_GetPosition(UUID, &position); }
        return position;
      }
      set 
      { 
        unsafe { InternalCalls.Transform_SetPosition(UUID, &value); } 
      }
    }

    public Vector3 WorldPosition
    {
      get 
      {
        Vector3 position;
        unsafe { InternalCalls.Transform_GetWorldPosition(UUID, &position); }
        return position;
      }
    }

    public Quaternion Rotation
    {
      get 
      {
        Quaternion rotation;
        unsafe { InternalCalls.Transform_GetRotation(UUID, &rotation); }
        return rotation;
      }
      set 
      { 
        unsafe { InternalCalls.Transform_SetRotation(UUID, &value); } 
      }
    }

    public Vector3 Right
    {
      get 
      {
        Vector3 right;
        unsafe { InternalCalls.Transform_GetRight(UUID, &right); }
        return right;
      }
    }

    public Vector3 Forward
    {
      get 
      {
        Vector3 forward;
        unsafe { InternalCalls.Transform_GetForward(UUID, &forward); }
        return forward;
      }
    }

    public Vector3 Up
    {
      get 
      {
        Vector3 up;
        unsafe { InternalCalls.Transform_GetUp(UUID, &up); }
        return up;
      }
    }

    public Vector3 Scale
    {
      get
      {
        Vector3 scale;
        unsafe { InternalCalls.Transform_GetScale(UUID, &scale); }
        return scale;
      }
      set
      {
        unsafe { InternalCalls.Transform_SetScale(UUID, &value); }
      }
    }

    public void LookAt(Vector3 target)
    {
      unsafe { InternalCalls.Transform_LookAt(UUID, &target); }
    }

    public override bool Equals(object? obj) {
      return obj is Transform other && Equals(other);
    }

		// Null-checked -- this is what "transform != null" (a plain reference null-check, not a value
		// comparison) actually calls via operator!= below, and Transform is a reference type (a
		// Component subclass), so a null "other" here is an expected, ordinary case, not a bug.
		public bool Equals(Transform? other)     {
      if (other is null) { return false; }
      return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
    }

		public override int GetHashCode()     {
      return (Position, Rotation, Scale).GetHashCode();
    }

		public static bool operator ==(Transform? left, Transform? right)     {
      if (ReferenceEquals(left, right)) { return true; }
      if (left is null || right is null) { return false; }
      return left.Equals(right);
    }

		public static bool operator !=(Transform? left, Transform? right)     {
      return !(left == right);
    }

	} // class Transform

  [Flags]
  public enum CollisionFlags : byte
  {
    None  = 0,
    Below = 1 << 0,
    Above = 1 << 1,
    Sides = 1 << 2
  } // enum CollisionFlags

  public class CharacterController : Component
  {
    public float Height 
    { 
      get
      {
        float height;
        unsafe { InternalCalls.CharacterController_GetHeight(UUID, &height); }
        return height;
      } 
    }

    public float Radius
    { 
      get
      {
        float radius;
        unsafe { InternalCalls.CharacterController_GetRadius(UUID, &radius); }
        return radius;
      } 
    }

    public float SlopeLimit
    { 
      get
      {
        float slopeLimit;
        unsafe { InternalCalls.CharacterController_GetSlopeLimit(UUID, &slopeLimit); }
        return slopeLimit;
      } 
    }

    public float StepOffset
    { 
      get
      {
        float stepOffset;
        unsafe { InternalCalls.CharacterController_GetStepOffset(UUID, &stepOffset); }
        return stepOffset;
      } 
    }

    public bool IsGrounded
    { 
      get
      {
        unsafe { return InternalCalls.CharacterController_GetIsGrounded(UUID); }
      } 
    }

    public CollisionFlags Flags
    { 
      get
      {
        CollisionFlags flags;
        unsafe { InternalCalls.CharacterController_GetFlags(UUID, (byte*)&flags); }
        return flags;
      } 
    }

    public void Move(Vector3 displacement)
    {
      unsafe { InternalCalls.CharacterController_Move(UUID, &displacement); }
    }
  }

} // namespace Sbx.Core
