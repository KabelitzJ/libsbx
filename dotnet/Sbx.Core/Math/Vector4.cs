using System.Runtime.InteropServices;

namespace Sbx.Core.Math
{

  /** Four floats -- a shader's float4 (per-instance custom data, matrix rows). */
  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Vector4
  {
    public float X;
    public float Y;
    public float Z;
    public float W;

    public Vector4(float x, float y, float z, float w)
    {
      X = x;
      Y = y;
      Z = z;
      W = w;
    }

    public override string ToString() => $"({X}, {Y}, {Z}, {W})";

  } // struct Vector4

} // namespace Sbx.Core.Math
