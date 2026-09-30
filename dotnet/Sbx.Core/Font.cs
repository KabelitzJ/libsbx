using System.Runtime.InteropServices;
using Sbx.Core.Math;

namespace Sbx.Core
{
  /**
   * One glyph, per 1 unit of font size, y down: its quad starts BearingX right of the pen and
   * BearingY below the baseline (negative: above), Width x Height big, and the pen then moves
   * Advance. Uv is the quad's rect in the font's atlas (x0, y0, x1, y1). Mirrors
   * scripting::font_glyph_data field for field.
   */
  [StructLayout(LayoutKind.Sequential)]
  public struct FontGlyph
  {
    public float UvX0;
    public float UvY0;
    public float UvX1;
    public float UvY1;
    public float Width;
    public float Height;
    public float BearingX;
    public float BearingY;
    public float Advance;
  } // struct FontGlyph

  /**
   * A TTF font as the engine cooks it: a single-channel SDF atlas (0.5 = glyph edge) plus glyph
   * metrics, codepoints 32..255 -- for scripts that lay text out themselves (e.g. text along a curve
   * in the world) instead of through UIText. Bind the atlas with Material.SetGenericTexture(int,
   * Font). A shared, cached asset like Texture2D.Load's; nothing to dispose.
   */
  public sealed class Font : Managed.INativeHandle
  {
    private readonly ulong _uuid;

    public ulong UUID => _uuid;

    ulong Managed.INativeHandle.Handle => _uuid;

    internal Font(ulong uuid)
    {
      _uuid = uuid;
    }

    public static object? FromHandle(ulong handle) => handle != 0 ? new Font(handle) : null;

    public static Font? Load(string path)
    {
      ulong uuid;
      unsafe { uuid = InternalCalls.Font_Load(path); }
      return uuid != 0 ? new Font(uuid) : null;
    }

    /** False for a while after Load: glyphs and atlas stream in on a background thread. TryGetGlyph fails until then. */
    public bool IsResident
    {
      get { unsafe { return InternalCalls.Font_IsResident(_uuid); } }
    }

    public bool TryGetGlyph(char character, out FontGlyph glyph)
    {
      FontGlyph result;
      bool found;
      unsafe { found = InternalCalls.Font_GetGlyph(_uuid, character, &result); }
      glyph = found ? result : default;
      return found;
    }

    /** Per 1 unit of font size. */
    public float LineHeight => Metrics().X;
    public float Ascent => Metrics().Y;
    public float Descent => Metrics().Z;

    private Vector3 Metrics()
    {
      Vector3 metrics;
      unsafe { InternalCalls.Font_GetMetrics(_uuid, &metrics); }
      return metrics;
    }

  } // class Font

} // namespace Sbx.Core
