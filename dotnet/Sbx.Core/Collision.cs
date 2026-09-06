using Sbx.Math;

namespace Sbx.Core
{

  /**
   * Payload for Behavior.OnCollisionEnter/OnCollisionExit/OnTriggerEnter/OnTriggerExit. Normal/Point
   * are only meaningful on an Enter callback -- an Exit means the pair stopped touching, so there's
   * no contact geometry left to report (both are default(Vector3) there).
   */
  public readonly struct Collision
  {

    public Node Other { get; }
    public Vector3 Normal { get; }
    public Vector3 Point { get; }

    public Collision(Node other, Vector3 normal, Vector3 point)
    {
      Other = other;
      Normal = normal;
      Point = point;
    }

  } // struct Collision

} // namespace Sbx.Core
