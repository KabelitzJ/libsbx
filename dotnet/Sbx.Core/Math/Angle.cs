namespace Sbx.Math
{
  public static class Angle
  {
    private const float Deg2Rad = MathF.PI / 180f;
    private const float Rad2Deg = 180f / MathF.PI;

    public static float ToRadians(float degrees)
    {
      return degrees * Deg2Rad;
    }

    public static float ToDegrees(float radians)
    {
      return radians * Rad2Deg;
    }

  } // class Angle

} // namespace Sbx.Math