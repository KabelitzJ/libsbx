using Sbx.Math;

namespace Sbx.Core
{

  public abstract class Component
  {

    public ulong Uuid { get; internal set; }

  }

  public class Tag : Component
  {

    public string? Value
    {
      get 
      { 
        unsafe { return InternalCalls.Tag_GetTag(Uuid); } 
      }
      set 
      { 
        unsafe { InternalCalls.Tag_SetTag(Uuid, value); } 
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
        unsafe { InternalCalls.Transform_GetPosition(Uuid, &position); }
        return position;
      }
      set 
      { 
        unsafe { InternalCalls.Transform_SetPosition(Uuid, &value); } 
      }
    }

    public Vector3 WorldPosition
    {
      get 
      {
        Vector3 position;
        unsafe { InternalCalls.Transform_GetWorldPosition(Uuid, &position); }
        return position;
      }
    }

    public Quaternion Rotation
    {
      get 
      {
        Quaternion rotation;
        unsafe { InternalCalls.Transform_GetRotation(Uuid, &rotation); }
        return rotation;
      }
      set 
      { 
        unsafe { InternalCalls.Transform_SetRotation(Uuid, &value); } 
      }
    }

    public Vector3 Right
    {
      get 
      {
        Vector3 right;
        unsafe { InternalCalls.Transform_GetRight(Uuid, &right); }
        return right;
      }
    }

    public Vector3 Forward
    {
      get 
      {
        Vector3 forward;
        unsafe { InternalCalls.Transform_GetForward(Uuid, &forward); }
        return forward;
      }
    }

    public Vector3 Up
    {
      get 
      {
        Vector3 up;
        unsafe { InternalCalls.Transform_GetUp(Uuid, &up); }
        return up;
      }
    }

		public Vector3 Scale;

    public void LookAt(Vector3 target)
    {
      unsafe { InternalCalls.Transform_LookAt(Uuid, &target); }
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
        unsafe { InternalCalls.CharacterController_GetHeight(Uuid, &height); }
        return height;
      } 
    }

    public float Radius
    { 
      get
      {
        float radius;
        unsafe { InternalCalls.CharacterController_GetRadius(Uuid, &radius); }
        return radius;
      } 
    }

    public float SlopeLimit
    { 
      get
      {
        float slopeLimit;
        unsafe { InternalCalls.CharacterController_GetSlopeLimit(Uuid, &slopeLimit); }
        return slopeLimit;
      } 
    }

    public float StepOffset
    { 
      get
      {
        float stepOffset;
        unsafe { InternalCalls.CharacterController_GetStepOffset(Uuid, &stepOffset); }
        return stepOffset;
      } 
    }

    public bool IsGrounded
    { 
      get
      {
        unsafe { return InternalCalls.CharacterController_GetIsGrounded(Uuid); }
      } 
    }

    public CollisionFlags Flags
    { 
      get
      {
        CollisionFlags flags;
        unsafe { InternalCalls.CharacterController_GetFlags(Uuid, (byte*)&flags); }
        return flags;
      } 
    }

    public void Move(Vector3 displacement)
    {
      unsafe { InternalCalls.CharacterController_Move(Uuid, &displacement); }
    }
  }

} // namespace Sbx.Core