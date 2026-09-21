namespace Sbx.Core.Math
{

  /**
   * Deterministic simplex noise (see the native sbx::math::noise). seed slices a fixed Z through
   * 3D simplex noise to get an independent field over the same (x, y) input -- pass different
   * seeds for decorrelated channels (e.g. separate X/Y/Z displacement) instead of one noise value
   * driving everything in lockstep.
   */
  public static class Noise
  {

    /** Single-octave simplex noise, roughly in [-1, 1]. */
    public static float Simplex(float x, float y, float z)
    {
      unsafe { return InternalCalls.Math_NoiseSimplex(x, y, z); }
    }

    /** Multi-octave (fractal Brownian motion) simplex noise -- smoother, larger-scale variation than Simplex. */
    public static float Fractal(float x, float y, float z, uint octaves, float lacunarity = 2.0f, float gain = 0.5f)
    {
      unsafe { return InternalCalls.Math_NoiseFractal(x, y, z, octaves, lacunarity, gain); }
    }

  } // class Noise

} // namespace Sbx.Core.Math
