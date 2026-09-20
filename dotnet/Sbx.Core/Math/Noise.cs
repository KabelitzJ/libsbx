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
    public static float Sample(float x, float y, int seed = 0)
    {
      unsafe { return InternalCalls.Math_NoiseSimplex(x, y, seed); }
    }

    /** Multi-octave (fractal Brownian motion) simplex noise -- smoother, larger-scale variation than Sample, e.g. for a heightmap. Roughly in [-1, 1]. */
    public static float Fractal(float x, float y, int seed, uint octaves)
    {
      unsafe { return InternalCalls.Math_NoiseFractal(x, y, seed, octaves); }
    }

  } // class Noise

} // namespace Sbx.Core.Math
