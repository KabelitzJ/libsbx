using System.Runtime.InteropServices;
using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Physics
{

  [Flags]
  public enum CollisionFlags : byte
  {
    None  = 0,
    Below = 1 << 0,
    Above = 1 << 1,
    Sides = 1 << 2
  } // enum CollisionFlags

  /** Mirrors scripting::character_controller_data field for field; CharacterController's properties read and write it whole. */
  [StructLayout(LayoutKind.Sequential)]
  public struct CharacterControllerSettings
  {
    /** Including both caps. */
    public float Height;
    public float Radius;
    /** Kept between the capsule and what it touches. */
    public float SkinWidth;
    /** Degrees; steeper surfaces are walls. */
    public float SlopeLimit;
    /** The tallest step it walks up. */
    public float StepOffset;
    public float MinMoveDistance;
    /** The capsule's center relative to the node, in world axes. */
    public Vector3 Center;
  } // struct CharacterControllerSettings

  /**
   * A kinematic, always-upright capsule moved only by Move/SimpleMove: it slides along walls, climbs
   * steps and slopes up to its limits and snaps down stairs. Rigidbodies are pushed off it; it is
   * never pushed back. Each surface a move runs into calls OnControllerColliderHit on this node's
   * scripts. Move it from FixedUpdate, once per step, alongside the physics it collides with.
   */
  public class CharacterController : Component
  {

    /** The whole configuration, as a copy: read, change, assign back. */
    public CharacterControllerSettings Settings
    {
      get
      {
        CharacterControllerSettings value;
        unsafe { InternalCalls.CharacterController_GetSettings(UUID, &value); }
        return value;
      }
      set
      {
        unsafe { InternalCalls.CharacterController_SetSettings(UUID, &value); }
      }
    }

    public float Height
    {
      get => Settings.Height;
      set { var settings = Settings; settings.Height = value; Settings = settings; }
    }

    public float Radius
    {
      get => Settings.Radius;
      set { var settings = Settings; settings.Radius = value; Settings = settings; }
    }

    public float SlopeLimit
    {
      get => Settings.SlopeLimit;
      set { var settings = Settings; settings.SlopeLimit = value; Settings = settings; }
    }

    public float StepOffset
    {
      get => Settings.StepOffset;
      set { var settings = Settings; settings.StepOffset = value; Settings = settings; }
    }

    public float SkinWidth
    {
      get => Settings.SkinWidth;
      set { var settings = Settings; settings.SkinWidth = value; Settings = settings; }
    }

    public Vector3 Center
    {
      get => Settings.Center;
      set { var settings = Settings; settings.Center = value; Settings = settings; }
    }

    /** Whether the last move ended standing on walkable ground. */
    public bool IsGrounded
    {
      get { unsafe { return InternalCalls.CharacterController_GetIsGrounded(UUID); } }
    }

    /** Which sides the last move touched. */
    public CollisionFlags Flags
    {
      get { unsafe { return (CollisionFlags)InternalCalls.CharacterController_GetFlags(UUID); } }
    }

    /** The last move's actual motion per second, after collisions. */
    public Vector3 Velocity
    {
      get
      {
        Vector3 velocity;
        unsafe { InternalCalls.CharacterController_GetVelocity(UUID, &velocity); }
        return velocity;
      }
    }

    /** Moves by displacement right away (no gravity applied); scale by Time.FixedDeltaTime yourself. */
    public CollisionFlags Move(Vector3 displacement)
    {
      unsafe { return (CollisionFlags)InternalCalls.CharacterController_Move(UUID, &displacement); }
    }

    /** Moves at speed (m/s) for one fixed step, ignoring its vertical part and applying gravity while airborne. Returns IsGrounded. */
    public bool SimpleMove(Vector3 speed)
    {
      unsafe { return InternalCalls.CharacterController_SimpleMove(UUID, &speed); }
    }

  } // class CharacterController

  /** A surface a CharacterController ran into during Move/SimpleMove. */
  public readonly struct ControllerColliderHit
  {

    public Node Other { get; }
    public Vector3 Point { get; }
    public Vector3 Normal { get; }
    public Vector3 MoveDirection { get; }
    public float MoveLength { get; }

    public ControllerColliderHit(Node other, Vector3 point, Vector3 normal, Vector3 moveDirection, float moveLength)
    {
      Other = other;
      Point = point;
      Normal = normal;
      MoveDirection = moveDirection;
      MoveLength = moveLength;
    }

  } // struct ControllerColliderHit

} // namespace Sbx.Core.Physics
