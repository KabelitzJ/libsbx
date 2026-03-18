using Sbx.Math;

namespace Demo
{
  internal static unsafe class InternalCalls
  {
    // Terrain
    internal static delegate* unmanaged<Vector2*, float*, void> Terrain_GetHeightAt;
    internal static delegate* unmanaged<Vector2*, void> Terrain_GetWorldSize;
    internal static delegate* unmanaged<float*, void> Terrain_GetCellSize;
    internal static delegate* unmanaged<Vector2*, float*, void> Terrain_GetSlopeAt;
    internal static delegate* unmanaged<float*, void> Terrain_GetSeaLevel;

  } // class InternalCalls

} // namespace Demo
