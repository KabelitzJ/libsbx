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

        hit = didHit ? new RaycastHit(new Node(nodeUuid), point, normal, distance) : default;

        return didHit;
      }
    }

  } // class Physics

} // namespace Sbx.Core.Physics
