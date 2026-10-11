using System.Runtime.InteropServices;
using Sbx.Core.Math;

namespace Sbx.Core.Physics
{

  /** Mirrors scripting::physics_shape_data field for field: a query shape and its world pose. Build one through the static helpers. */
  [StructLayout(LayoutKind.Sequential)]
  internal struct PhysicsShape
  {
    public uint Kind;
    public float Radius;
    public float HalfHeight;
    public Vector3 HalfExtents;
    public Vector3 Position;
    public Quaternion Rotation;

    public static PhysicsShape Sphere(Vector3 center, float radius) => new()
    {
      Kind = 0,
      Radius = radius,
      Position = center,
      Rotation = Quaternion.Identity,
    };

    public static PhysicsShape Box(Vector3 center, Vector3 halfExtents, Quaternion orientation) => new()
    {
      Kind = 1,
      HalfExtents = halfExtents,
      Position = center,
      Rotation = orientation,
    };

    /** height is the full height including both caps, like CharacterController.Height; the capsule's axis is its local Y. */
    public static PhysicsShape Capsule(Vector3 center, float radius, float height, Quaternion orientation) => new()
    {
      Kind = 2,
      Radius = radius,
      HalfHeight = System.Math.Max(height * 0.5f - radius, 0.0f),
      Position = center,
      Rotation = orientation,
    };

  } // struct PhysicsShape

} // namespace Sbx.Core.Physics
