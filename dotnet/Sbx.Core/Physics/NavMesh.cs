using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Physics
{

  /** Scene-wide navmesh baking/queries -- not a Component, same shape as Physics. */
  public static class NavMesh
  {

    /**
     * Bakes the navmesh from the active scene's current static geometry right now (see the
     * native physics_module::bake_navmesh) -- replaces any previously baked navmesh. Returns
     * false if the bake produced no usable polygons (e.g. no static colliders in the scene).
     */
    public static bool Bake(
      float agentRadius = 0.3f,
      float agentHeight = 1.8f,
      float agentMaxSlope = 45.0f,
      float agentMaxClimb = 0.4f,
      float cellSize = 0.2f,
      float cellHeight = 0.2f,
      float regionMinSize = 8.0f,
      float regionMergeSize = 20.0f,
      float edgeMaxLength = 12.0f,
      float edgeMaxError = 1.3f,
      int vertsPerPoly = 6)
    {
      unsafe
      {
        return InternalCalls.Nav_Bake(agentRadius, agentHeight, agentMaxSlope, agentMaxClimb, cellSize, cellHeight, regionMinSize, regionMergeSize, edgeMaxLength, edgeMaxError, vertsPerPoly);
      }
    }

    public static bool HasNavMesh
    {
      get
      {
        unsafe
        {
          return InternalCalls.Nav_HasNavMesh();
        }
      }
    }

    /**
     * The closest point on the navmesh to `point` (nearest polygon, then clamped to it).
     * Returns false (result left as `point`) if there's no navmesh yet.
     */
    public static bool SamplePosition(Vector3 point, out Vector3 result)
    {
      unsafe
      {
        Vector3 sampled;

        var found = InternalCalls.Nav_SamplePosition(&point, &sampled);

        result = found ? sampled : point;

        return found;
      }
    }

  } // class NavMesh

} // namespace Sbx.Core.Physics
