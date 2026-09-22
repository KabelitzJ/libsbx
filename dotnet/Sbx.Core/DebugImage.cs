using Sbx.Core.Math;

namespace Sbx.Core
{

  /** Dumps in-memory pixel data (e.g. Texture2D.ReadPixels' output) to a real PNG file on disk, for visually inspecting a compute-baked texture instead of only reading it back into more script code. Debugging-only -- path is a raw filesystem path, not an asset. */
  public static class DebugImage
  {

    /** pixels must be width*height long, row-major. Each channel is clamped to 0..1 before encoding to a byte. Creates the destination directory if needed. Returns false (and logs) on failure. */
    public static bool SavePng(string path, int width, int height, Color[] pixels)
    {
      if (pixels.Length != width * height)
      {
        Logger.Error("DebugImage.SavePng: pixels.Length ({0}) does not match width*height ({1})", pixels.Length, width * height);
        return false;
      }

      var bytes = new byte[width * height * 4];

      for (var i = 0; i < pixels.Length; i++)
      {
        var pixel = pixels[i];

        bytes[i * 4 + 0] = ToByte(pixel.R);
        bytes[i * 4 + 1] = ToByte(pixel.G);
        bytes[i * 4 + 2] = ToByte(pixel.B);
        bytes[i * 4 + 3] = ToByte(pixel.A);
      }

      unsafe
      {
        fixed (byte* ptr = bytes)
        {
          return InternalCalls.Debug_WritePng(path, (uint)width, (uint)height, ptr);
        }
      }
    }

    private static byte ToByte(float channel)
    {
      return (byte)(System.Math.Clamp(channel, 0.0f, 1.0f) * 255.0f);
    }

  } // class DebugImage

} // namespace Sbx.Core
