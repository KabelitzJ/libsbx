using Sbx.Core.Math;

namespace Sbx.Core
{

  /** Immediate-mode debug lines, drawn for one frame and occluded by opaque geometry. Call every frame the shape should stay visible. */
  public static class DebugDraw
  {

    public static void Line(Vector3 start, Vector3 end, Color color)
    {
      unsafe { InternalCalls.DebugDraw_Line(&start, &end, &color); }
    }

    public static void WireSphere(Vector3 center, float radius, Color color)
    {
      unsafe { InternalCalls.DebugDraw_WireSphere(&center, radius, &color); }
    }

    public static void WireBox(Vector3 center, Vector3 halfExtents, Color color)
    {
      WireBox(center, Quaternion.Identity, halfExtents, color);
    }

    public static void WireBox(Vector3 center, Quaternion rotation, Vector3 halfExtents, Color color)
    {
      unsafe { InternalCalls.DebugDraw_WireBox(&center, &rotation, &halfExtents, &color); }
    }

    /** A ring in the plane perpendicular to normal; Vector3.Up gives a ground-aligned circle. */
    public static void WireCircle(Vector3 center, Vector3 normal, float radius, Color color)
    {
      unsafe { InternalCalls.DebugDraw_WireCircle(&center, &normal, radius, &color); }
    }

  } // class DebugDraw

} // namespace Sbx.Core
