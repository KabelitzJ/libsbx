using Sbx.Math;

namespace Sbx.Core
{

  /** Result of a successful Physics.Raycast call. */
  public readonly struct RaycastHit
  {

    public Node Node { get; }
    public Vector3 Point { get; }
    public Vector3 Normal { get; }
    public float Distance { get; }

    public RaycastHit(Node node, Vector3 point, Vector3 normal, float distance)
    {
      Node = node;
      Point = point;
      Normal = normal;
      Distance = distance;
    }

  } // struct RaycastHit

} // namespace Sbx.Core
