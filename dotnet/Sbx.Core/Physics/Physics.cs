using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Physics
{

  /** Scene-wide physics queries -- not a Component, unlike Rigidbody/MeshRenderer/etc. */
  public static class Physics
  {

    /**
     * Raycasts against the active scene's broadphase: shape_collider/convex mesh_collider
     * primitives and terrain (see the native physics_module::raycast). Returns false (hit left
     * default) if nothing was hit within maxDistance. Hits every layer -- see the overload below
     * to filter by layerMask.
     */
    public static bool Raycast(Ray ray, float maxDistance, out RaycastHit hit)
    {
      return Raycast(ray, maxDistance, out hit, LayerMask.Everything);
    }

    /** Same as the other Raycast overload, but only considers colliders on a layer included in layerMask. */
    public static bool Raycast(Ray ray, float maxDistance, out RaycastHit hit, LayerMask layerMask)
    {
      unsafe
      {
        ulong nodeUuid;
        Vector3 point;
        Vector3 normal;
        float distance;

        var didHit = InternalCalls.Physics_Raycast(&ray, maxDistance, layerMask.Value, &nodeUuid, &point, &normal, &distance);

        hit = didHit ? new RaycastHit(Node.Get(nodeUuid)!, point, normal, distance) : default;

        return didHit;
      }
    }

    /** Every node whose collider overlaps the sphere. Hits every layer -- see the overload below to filter by layerMask. */
    public static Node[] OverlapSphere(Vector3 center, float radius)
    {
      return OverlapSphere(center, radius, LayerMask.Everything);
    }

    /** Every node on a layer in layerMask whose collider overlaps the sphere. Allocates; prefer OverlapSphereNonAlloc for per-frame queries. */
    public static Node[] OverlapSphere(Vector3 center, float radius, LayerMask layerMask)
    {
      Span<ulong> uuids = stackalloc ulong[64];

      var total = QueryOverlapSphere(center, radius, layerMask, uuids);

      if (total > uuids.Length)
      {
        uuids = new ulong[total];
        total = System.Math.Min(QueryOverlapSphere(center, radius, layerMask, uuids), uuids.Length);
      }

      var results = new Node[total];

      for (var i = 0; i < total; ++i)
      {
        results[i] = Node.Get(uuids[i])!;
      }

      return results;
    }

    /** Fills results with up to results.Length overlapping nodes and returns how many were written. Allocates nothing for buffers up to 256. */
    public static int OverlapSphereNonAlloc(Vector3 center, float radius, Node[] results, LayerMask layerMask)
    {
      Span<ulong> uuids = results.Length <= 256 ? stackalloc ulong[results.Length] : new ulong[results.Length];

      var written = System.Math.Min(QueryOverlapSphere(center, radius, layerMask, uuids), results.Length);

      for (var i = 0; i < written; ++i)
      {
        results[i] = Node.Get(uuids[i])!;
      }

      return written;
    }

    /** Sweeps a sphere from origin along direction; hit.Distance is how far it travels before touching. */
    public static bool SphereCast(Vector3 origin, float radius, Vector3 direction, float maxDistance, out RaycastHit hit)
    {
      return ShapeCast(PhysicsShape.Sphere(origin, radius), direction, maxDistance, LayerMask.Everything, out hit);
    }

    public static bool SphereCast(Vector3 origin, float radius, Vector3 direction, float maxDistance, out RaycastHit hit, LayerMask layerMask)
    {
      return ShapeCast(PhysicsShape.Sphere(origin, radius), direction, maxDistance, layerMask, out hit);
    }

    /** Sweeps a box (orientation held fixed) from center along direction. */
    public static bool BoxCast(Vector3 center, Vector3 halfExtents, Quaternion orientation, Vector3 direction, float maxDistance, out RaycastHit hit)
    {
      return ShapeCast(PhysicsShape.Box(center, halfExtents, orientation), direction, maxDistance, LayerMask.Everything, out hit);
    }

    public static bool BoxCast(Vector3 center, Vector3 halfExtents, Quaternion orientation, Vector3 direction, float maxDistance, out RaycastHit hit, LayerMask layerMask)
    {
      return ShapeCast(PhysicsShape.Box(center, halfExtents, orientation), direction, maxDistance, layerMask, out hit);
    }

    /** Sweeps a capsule (local Y axis, height including caps) from center along direction. */
    public static bool CapsuleCast(Vector3 center, float radius, float height, Quaternion orientation, Vector3 direction, float maxDistance, out RaycastHit hit)
    {
      return ShapeCast(PhysicsShape.Capsule(center, radius, height, orientation), direction, maxDistance, LayerMask.Everything, out hit);
    }

    public static bool CapsuleCast(Vector3 center, float radius, float height, Quaternion orientation, Vector3 direction, float maxDistance, out RaycastHit hit, LayerMask layerMask)
    {
      return ShapeCast(PhysicsShape.Capsule(center, radius, height, orientation), direction, maxDistance, layerMask, out hit);
    }

    /** Every node whose collider overlaps the box. Allocates. */
    public static Node[] OverlapBox(Vector3 center, Vector3 halfExtents, Quaternion orientation, LayerMask layerMask)
    {
      return OverlapShape(PhysicsShape.Box(center, halfExtents, orientation), layerMask);
    }

    public static Node[] OverlapBox(Vector3 center, Vector3 halfExtents, Quaternion orientation)
    {
      return OverlapBox(center, halfExtents, orientation, LayerMask.Everything);
    }

    /** Every node whose collider overlaps the capsule (local Y axis, height including caps). Allocates. */
    public static Node[] OverlapCapsule(Vector3 center, float radius, float height, Quaternion orientation, LayerMask layerMask)
    {
      return OverlapShape(PhysicsShape.Capsule(center, radius, height, orientation), layerMask);
    }

    public static Node[] OverlapCapsule(Vector3 center, float radius, float height, Quaternion orientation)
    {
      return OverlapCapsule(center, radius, height, orientation, LayerMask.Everything);
    }

    private static bool ShapeCast(PhysicsShape shape, Vector3 direction, float maxDistance, LayerMask layerMask, out RaycastHit hit)
    {
      unsafe
      {
        ulong nodeUuid;
        Vector3 point;
        Vector3 normal;
        float distance;

        var didHit = InternalCalls.Physics_ShapeCast(&shape, &direction, maxDistance, layerMask.Value, &nodeUuid, &point, &normal, &distance);

        hit = didHit ? new RaycastHit(Node.Get(nodeUuid)!, point, normal, distance) : default;

        return didHit;
      }
    }

    private static Node[] OverlapShape(PhysicsShape shape, LayerMask layerMask)
    {
      Span<ulong> uuids = stackalloc ulong[64];

      var total = QueryOverlapShape(shape, layerMask, uuids);

      if (total > uuids.Length)
      {
        uuids = new ulong[total];
        total = System.Math.Min(QueryOverlapShape(shape, layerMask, uuids), uuids.Length);
      }

      var results = new Node[total];

      for (var i = 0; i < total; ++i)
      {
        results[i] = Node.Get(uuids[i])!;
      }

      return results;
    }

    private static int QueryOverlapShape(PhysicsShape shape, LayerMask layerMask, Span<ulong> uuids)
    {
      unsafe
      {
        fixed (ulong* uuidsPtr = uuids)
        {
          return (int)InternalCalls.Physics_OverlapShape(&shape, layerMask.Value, uuidsPtr, (uint)uuids.Length);
        }
      }
    }

    /** Writes up to uuids.Length hits and returns the total overlap count, which may be larger. */
    private static int QueryOverlapSphere(Vector3 center, float radius, LayerMask layerMask, Span<ulong> uuids)
    {
      unsafe
      {
        fixed (ulong* uuidsPtr = uuids)
        {
          return (int)InternalCalls.Physics_OverlapSphere(&center, radius, layerMask.Value, uuidsPtr, (uint)uuids.Length);
        }
      }
    }

  } // class Physics

} // namespace Sbx.Core.Physics
