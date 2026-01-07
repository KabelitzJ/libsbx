using System.Runtime.InteropServices;

namespace Sbx.Math
{

  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Ray
  {
    public Vector3 Origin;
    public Vector3 Direction ;

    public Ray(Vector3 origin, Vector3 direction)
    {
      Origin = origin;
      Direction = direction.Normalized();
    }

    public Vector3 GetPoint(float t)
    {
      return Origin + Direction * t;
    }

    public override string ToString()
    {
      return $"Ray[Origin={Origin}, Direction={Direction}]";
    }

  } // struct Ray
}
