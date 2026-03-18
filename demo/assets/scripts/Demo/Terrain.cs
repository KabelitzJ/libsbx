using Sbx.Math;

namespace Demo
{
  public static class Terrain
  {
    public static float GetHeightAt(float worldX, float worldZ)
    {
      var coord = new Vector2(worldX, worldZ);
      float height;

      unsafe
      {
        InternalCalls.Terrain_GetHeightAt(&coord, &height);
      }

      return height;
    }

    public static Vector2 GetWorldSize()
    {
      Vector2 size;

      unsafe
      {
        InternalCalls.Terrain_GetWorldSize(&size);
      }

      return size;
    }

    public static float GetCellSize()
    {
      float cellSize;

      unsafe
      {
        InternalCalls.Terrain_GetCellSize(&cellSize);
      }

      return cellSize;
    }

    public static float GetSlopeAt(int cellX, int cellZ)
    {
      var cell = new Vector2(cellX, cellZ);
      float slope;

      unsafe
      {
        InternalCalls.Terrain_GetSlopeAt(&cell, &slope);
      }

      return slope;
    }

    public static float SeaLevel
    {
      get
      {
        float seaLevel;

        unsafe
        {
          InternalCalls.Terrain_GetSeaLevel(&seaLevel);
        }

        return seaLevel;
      }
    }

  } // class Terrain

} // namespace Demo
