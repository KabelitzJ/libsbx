using System.Runtime.InteropServices;

namespace Sbx.Math
{
  // Mirrors sbx::math::color's field layout exactly (four sequential floats) -- see
  // libsbx/math/color.hpp.
  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Color
  {

    public static Color White = new Color(1, 1, 1, 1);
    public static Color Black = new Color(0, 0, 0, 1);

    public float R;
    public float G;
    public float B;
    public float A;

    public Color(float r, float g, float b, float a = 1)
    {
      R = r;
      G = g;
      B = b;
      A = a;
    }

    public override string ToString()
    {
      return $"Color[R={R}, G={G}, B={B}, A={A}]";
    }

  } // struct Color
}
