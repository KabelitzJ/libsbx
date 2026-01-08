using System.Runtime.InteropServices;
using Sbx.Core;

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
      Logger.Debug("Ray: Origin={0}, Direction={1}", ray.Origin, ray.Direction);

      float denom = Vector3.Dot(Normal, ray.Direction);

      Logger.Debug("Denominator: {0}", denom);

      if (MathF.Abs(denom) > 1e-6f)
      {
        t = -(Vector3.Dot(Normal, ray.Origin) + Distance) / denom;

        Logger.Debug("Ray intersects plane at t={0}", t);

        return t >= 0.0f;
      }

      t = 0f;

      Logger.Debug("Ray is parallel to plane");

      return false;
    }

    public override string ToString()
    {
      return $"Plane[Normal={Normal}, Distance={Distance}]";
    }

  } // class Plane

} // namespace Sbx.Math
