using System.Runtime.InteropServices;

namespace Sbx.Math
{

  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Plane
  {

    public Vector3 Normal { get; set; }
    public float Distance { get; set; }

    public Plane(Vector3 normal, Vector3 point)
    {
      Normal = normal.Normalized();
      Distance = -Vector3.Dot(Normal, point);
    }

    public Plane(Vector3 normal, float distance)
    {
      Normal = normal.Normalized();
      Distance = distance;
    }

    public float GetDistanceToPoint(Vector3 point)
    {
      return Vector3.Dot(Normal, point) + Distance;
    }

    public bool IsPointInFront(Vector3 point)
    {
      return GetDistanceToPoint(point) > 0f;
    }

    // Ray-plane intersection
    public bool Raycast(Ray ray, out float t)
    {
      float denom = Vector3.Dot(Normal, ray.Direction);

      if (MathF.Abs(denom) > 1e-6f)
      {
        t = -(Vector3.Dot(Normal, ray.Origin) + Distance) / denom;

        return t >= 0f;
      }

      t = 0f;

      return false;
    }

    public override string ToString()
    {
      return $"Plane[Normal={Normal}, Distance={Distance}]";
    }

  } // class Plane

} // namespace Sbx.Math