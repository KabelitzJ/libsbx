using Sbx.Math;

namespace Sbx.Core
{

  /** Elevation/slope sampling against the active scene's terrain (see the native terrain_module). Not a Component -- there's exactly one terrain heightmap per scene in v1. */
  public static class Terrain
  {

    /** (Re)generates the scene's terrain, replacing whatever a previous call generated. */
    public static void Generate(uint width = 129, uint depth = 129, float cellSize = 1.0f, float frequency = 0.02f, float amplitude = 10.0f, uint octaves = 4)
    {
      unsafe { InternalCalls.Terrain_Generate(width, depth, cellSize, frequency, amplitude, octaves); }
    }

    /** 0 if no terrain has been generated yet. */
    public static float SampleHeight(Vector2 worldXZ)
    {
      unsafe
      {
        float height;
        InternalCalls.Terrain_SampleHeight(&worldXZ, &height);
        return height;
      }
    }

    /** +Y if no terrain has been generated yet. */
    public static Vector3 SampleNormal(Vector2 worldXZ)
    {
      unsafe
      {
        Vector3 normal;
        InternalCalls.Terrain_SampleNormal(&worldXZ, &normal);
        return normal;
      }
    }

  } // class Terrain

} // namespace Sbx.Core
