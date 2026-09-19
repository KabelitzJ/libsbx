using System.Runtime.InteropServices;

namespace Sbx.Core.Math
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

    public static Color Lerp(Color a, Color b, float t)
    {
      t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

      return new Color(a.R + (b.R - a.R) * t, a.G + (b.G - a.G) * t, a.B + (b.B - a.B) * t, a.A + (b.A - a.A) * t);
    }

  } // struct Color
}
