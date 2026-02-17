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
      get 
      { 
        unsafe { return InternalCalls.Tag_GetTag(Node); } 
      }
      set 
      { 
        unsafe { InternalCalls.Tag_SetTag(Node, value); } 
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
        unsafe { InternalCalls.Transform_GetPosition(Node, &position); }
        return position;
      }
      set 
      { 
        unsafe { InternalCalls.Transform_SetPosition(Node, &value); } 
      }
    }

    public Vector3 WorldPosition
    {
      get 
      {
        Vector3 position;
        unsafe { InternalCalls.Transform_GetWorldPosition(Node, &position); }
        return position;
      }
    }

    public Quaternion Rotation
    {
      get 
      {
        Quaternion rotation;
        unsafe { InternalCalls.Transform_GetRotation(Node, &rotation); }
        return rotation;
      }
      set 
      { 
        unsafe { InternalCalls.Transform_SetRotation(Node, &value); } 
      }
    }

    public Vector3 Right
    {
      get 
      {
        Vector3 right;
        unsafe { InternalCalls.Transform_GetRight(Node, &right); }
        return right;
      }
    }

    public Vector3 Forward
    {
      get 
      {
        Vector3 forward;
        unsafe { InternalCalls.Transform_GetForward(Node, &forward); }
        return forward;
      }
    }

    public Vector3 Up
    {
      get 
      {
        Vector3 up;
        unsafe { InternalCalls.Transform_GetUp(Node, &up); }
        return up;
      }
    }

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

	} // class Transform

  [Flags]
  public enum CollisionFlags : byte
  {
    None  = 0,
    Below = 1 << 0,
    Above = 1 << 1,
    Sides = 1 << 2
  } // enum CollisionFlags

  public struct MoveResult
  {
    public Vector3 Position;
    public CollisionFlags Flags;
    public Vector3 GroundNormal;
    public bool Grounded;
  } // struct MoveResult

  public class CharacterController : Component
  {
    public float Height 
    { 
      get
      {
        float height;
        unsafe { InternalCalls.CharacterController_GetHeight(Node, &height); }
        return height;
      } 
    }

    public float Radius
    { 
      get
      {
        float radius;
        unsafe { InternalCalls.CharacterController_GetRadius(Node, &radius); }
        return radius;
      } 
    }

    public float SlopeLimit
    { 
      get
      {
        float slopeLimit;
        unsafe { InternalCalls.CharacterController_GetSlopeLimit(Node, &slopeLimit); }
        return slopeLimit;
      } 
    }

    public float StepOffset
    { 
      get
      {
        float stepOffset;
        unsafe { InternalCalls.CharacterController_GetStepOffset(Node, &stepOffset); }
        return stepOffset;
      } 
    }


    public MoveResult Move(Vector3 position, Vector3 displacement)
    {
      MoveResult result;
      unsafe { InternalCalls.CharacterController_Move(Node, &position, &displacement, &result); }
      return result;
    }
  }

} // namespace Sbx.Core