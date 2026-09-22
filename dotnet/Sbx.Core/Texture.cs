using Sbx.Core.Math;

namespace Sbx.Core
{

  /**
   * Read-only, CPU-side pixel sampling of an image file -- decoded once and cached natively by
   * path, independent of the GPU texture/material pipeline (which keeps no CPU-side pixels once a
   * texture is uploaded). For script-side procedural generation that needs to read source art
   * directly, not for anything rendered.
   */
  public static class Texture
  {

    /** Bilinear sample at normalized (u, v). Returns Color.White (and logs a native-side error) if the file can't be found/decoded. */
    public static Color SampleBilinear(string path, float u, float v)
    {
      unsafe
      {
        Color color;

        return InternalCalls.Texture_SampleBilinear(path, u, v, &color) ? color : Color.White;
      }
    }

  } // class Texture

} // namespace Sbx.Core
